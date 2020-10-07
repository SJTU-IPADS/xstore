#pragma once

#include "./bootstrap.hh"
#include "./page_fetcher.hpp"

#include "../cli/fclient.hpp"

//#include "../deps/new_cache/cache.hh"

namespace fstore {

const usize c_max_page_to_fetch = 16;

extern __thread u64 data_transfered;
extern __thread u64 err_num;

#define COUNT_MISS 0 // only record the cache misses in data_transfered
#define BYPASS_CACHE 0

#define LRU 0

#define COUNT_LAT 0

class XCachedClient
{
public:
  LRModel<> dispatcher;
  usize num_total_model;
  usize max_addr;

#if COUNT_LAT
  u64 model_time;
  u64 rdma_index;
  u64 total_time;
  u64 rdma_value;

  void reset_lat() {
    model_time = 0;
    rdma_index = 0;
    total_time = 0;
    rdma_value = 0;
  }
#endif


#if LRU
  ::rcache::Cache<usize, CBM *> model_cache;
#else
  //std::unordered_map<usize, CBM *> model_cache;
  const usize max_cached_entry;
  usize cur_cached = 0;
  std::vector<CBM *> model_cache;
#endif

  u64 base_addr; // RDMA-base addr, for translating remote virtual address to
                 // RDMA offset
  u64 submodels_addr; // submodel-base addr

  u64 page_start;

  RCQP *qp = nullptr;

  static XCachedClient *create(int table_id, usize max_cached_num, RPC &rpc, const Addr &server_addr, u64 page_start, R2_ASYNC) {
    char reply_buf[1024];
    memset(reply_buf, 0, 1024);

    auto& factory = rpc.get_buf_factory();
    auto send_buf = factory.alloc(128);

    // first we fetch the Meta data
    Marshal<TableModel>::serialize_to({ .id = table_id }, send_buf);

    auto ret = rpc.call({ .cor_id = R2_COR_ID(), .dest = server_addr },
                        MODEL_META,
                        { .send_buf = send_buf,
                          .len = sizeof(TableModel),
                          .reply_buf = reply_buf,
                          .reply_cnt = 1 });
    R2_PAUSE;
    factory.dealloc(send_buf);

    // parse the reply
    ModelMeta meta = Marshal<ModelMeta>::extract(reply_buf);
    LOG(0) << "sanity check meta at client : " << meta.dispatcher_sz << " "
           << meta.submodel_buf_addr << " " << meta.num_submodel << " "
           << meta.max_addr;

    return new XCachedClient(
      std::string(reply_buf + sizeof(ModelMeta), meta.dispatcher_sz),
      meta.num_submodel,
      max_cached_num,
      meta.max_addr,
      meta.base_addr,
      meta.submodel_buf_addr,
      page_start);
  }

  XCachedClient(const std::string &d, usize total, int max_cached_num, usize max_addr, u64 b, u64 s,u64 page_start) :
    num_total_model(total),
    max_addr(max_addr),
#if LRU
    model_cache(max_cached_num),
#else
    max_cached_entry(max_cached_num),
    model_cache(max_cached_num, nullptr),
#endif
    base_addr(b),
    submodels_addr(s),
    page_start(page_start)
  {
    dispatcher.from_serialize(d);
  }

  usize select_submodel(const u64& key)
  {
    auto predict = static_cast<int>(dispatcher.predict(key));
    usize ret = 0;
    if (predict < 0) {
      ret = 0;
    } else if (predict >= max_addr) {
      ret = num_total_model - 1;
    } else {
      ret = static_cast<usize>((static_cast<double>(predict) / max_addr) *
                               num_total_model);
    }
    ASSERT(ret < num_total_model)
      << "predict: " << predict << " " << num_total_model;
    // LOG(4) << "select " << ret << " for " << key << " with predict: " <<
    // predict;
    return ret;
  }

  SearchCode get_direct(const u64 &key, char *local_buf, R2_ASYNC) {
    ASSERT(false);
#if COUNT_LAT
    this->reset_lat();
    u64 start_ts = read_tsc();
#endif
    auto s = this->select_submodel(key);
    //auto model = this->get_submodel(s, local_buf, R2_ASYNC_WAIT);
    CBM temp;
    auto model = this->get_model(s, temp, local_buf,R2_ASYNC_WAIT);
    ASSERT(model->page_table.size() != 0) << model << " " << &temp << " " << s;

    auto page_range = model->get_page_span(key);
    err_num = std::get<1>(page_range) - std::get<0>(page_range) + 1;

#if COUNT_LAT
    this->model_time += read_tsc() - start_ts;
    u64 index_s = read_tsc();
#endif
    std::vector<u64> page_addrs;
    auto res = this->fetch_pages(
                                 s, model, page_range, page_addrs, local_buf, R2_ASYNC_WAIT);
    if (res != Ok) {
      if (res == Fallback) {
        //LOG(4) << "fallback in fetch pageS !"; sleep(1);
      }
      return res;
    }

#if COUNT_LAT
    this->rdma_index += read_tsc() - index_s;
    u64 s_start = read_tsc();
#endif
    // 2. search
    auto ret =
      search_within_pages(model, page_range,page_addrs, key, local_buf);
    if (ret) {

#if COUNT_LAT
      model_time += read_tsc() - s_start;
#endif
      auto code = std::get<0>(ret.value());
      if (code != Ok) {
        if (code == Fallback) {
          //LOG(4) << "fallback in search within pages"; sleep(1);
        }
        return code;
      }
      // found
      auto page_id = std::get<1>(ret.value());
      auto idx = std::get<2>(ret.value());

#if COUNT_LAT
      u64 v_s = read_tsc();
#endif

      // compute value address
      auto val_addr =
        this->page_to_rdma(model->lookup_page_phy_addr(page_id)) +
        Leaf::value_offset(idx);

      // issue the RDMA
      ::r2::rdma::SROp op(qp);
      op.set_read().set_remote_addr(val_addr).set_payload(
        local_buf, ValType::get_payload());
      auto res = op.execute(R2_ASYNC_WAIT);
      ASSERT(std::get<0>(res) == SUCC);
#if ! COUNT_MISS
      data_transfered += 1;
#endif

#if COUNT_LAT
      rdma_value += read_tsc() - v_s;
      total_time += read_tsc() - start_ts;
#endif
      return Ok;

    } else {
      // ASSERT(false);
      return None;
    }
    return Err;
  }

  inline u64 virt_to_rdma(const u64& v) { return v - base_addr; }

  void fill_all_submodels(char *local_buf, R2_ASYNC) {
    CBM temp;
    this->model_cache.resize(this->num_total_model);
    for(uint i = 0;i < this->num_total_model;++i) {
      this->get_model(i, temp, local_buf, R2_ASYNC_WAIT);
      ASSERT(temp.page_table.size() != 0);
    }
  }

  CBM *get_model(const usize &idx, CBM &temp, char *local_buf,R2_ASYNC) {
#if !BYPASS_CACHE
#if LRU
    auto cached_res = this->model_cache.get(idx);
    if (cached_res) {
      return cached_res.value();
    }
#else
    if (idx < model_cache.size() && model_cache[idx] != nullptr) {
      return model_cache[idx];
    }
#endif
#endif
    // fetch
    auto submodel_sz = Serializer::sizeof_submodel<LRModel<>>() + sizeof(u64) + sizeof(u64);

    ::r2::rdma::SROp op(this->qp);
    op.set_read()
      .set_remote_addr(this->virt_to_rdma(submodels_addr +
                                          static_cast<u64>(idx) * submodel_sz))
      .set_payload(local_buf, submodel_sz);
    auto res = op.execute(R2_ASYNC_WAIT);
    ASSERT(std::get<0>(res) == SUCC);

    data_transfered += 1;

#if !BYPASS_CACHE
#if LRU
    // update the cache
    auto sbm = new CBM();
    Serializer::extract_submodel_to(local_buf, *sbm);
    auto ret = this->model_cache.put(idx, sbm);
    if (ret) {
      delete ret.value();
    }
    return sbm;
#else
    Option<u64> ext = {};
    auto sbm = new CBM();
    Serializer::extract_submodel_to(local_buf, *sbm ,ext);
    //ASSERT(!ext) << "ext not supported in cached mod yet";
    if (ext) {
      op.set_read()
        .set_remote_addr(this->virt_to_rdma(ext.value()))
        .set_payload(local_buf, sbm->page_table.size() * sizeof(u64));
      auto res = op.execute(R2_ASYNC_WAIT);
      ASSERT(std::get<0>(res) == SUCC);
      for (uint i = 0; i < sbm->page_table.size(); ++i) {
        auto addr = Marshal<u64>::extract_with_inc(local_buf);
        sbm->page_table[i] = addr;
      }
    } else {
      //ASSERT(false);
    }

    if (cur_cached  < max_cached_entry && idx < model_cache.size()) {
    //if (1) {
      cur_cached += 1;
      //this->model_cache.insert(std::make_pair(idx,sbm));
      ASSERT(idx < model_cache.size()) << idx << "; " << model_cache.size();
      model_cache[idx] = sbm;
      ASSERT(sbm->page_table.size() > 0);
      return sbm;
    } else {
      ASSERT(false);
    }
#endif
#endif
    return &temp;
  }


  inline u64 page_to_rdma(const u64& v)
  {
    return sizeof(Leaf) * v + page_start;
  }

  const usize read_sz = Leaf::value_offset(0);
  SearchCode fetch_pages(usize model_id,
                         CBM *model,
                         const std::pair<u64, u64>& span,
                         std::vector<u64> &page_addrs,
                         char* buf,
                         R2_ASYNC)
  {

    if ((std::get<1>(span) - std::get<0>(span) + 1) >= c_max_page_to_fetch) {
      return Unsafe;
    }

    ::fstore::X::RemoteFetcher<c_max_page_to_fetch> fetcher(qp, R2_COR_ID());

    int pending = 0;
    for (uint p = std::get<0>(span); p <= std::get<1>(span); ++p) {
      auto page_table_entry_opt = model->lookup_page_entry(p);
      if (page_table_entry_opt) {

        auto page_table_entry = page_table_entry_opt.value();
        r2::compile_fence();
#if 1
        if (page_table_entry == INVALID_PAGE_ID) {
          //LOG(4) << "fallback model: " << model_id << " at entry: !!!" << p; sleep(1);
          return Fallback;
        }
#endif
        auto page_addr =
          this->page_to_rdma(SeqEncode::decode_id(page_table_entry));
        page_addrs.push_back(page_addr);

        pending = fetcher.add(
          page_addr, read_sz, buf + (p - std::get<0>(span)) * read_sz);
      } else {
#if 0
        if (model->seq != model_seq || p >= max_page_entries_to_serialize) {
          return Unsafe;
        }
#endif
        ASSERT(false) << "page span: " << p << " from: " << std::get<0>(span)
                      << " " << std::get<1>(span) << " for model: " << model_id
                      << " check sz: " << std::get<1>(span) - std::get<0>(span)
                      << "; unsafe? "
                      << ((std::get<1>(span) - std::get<0>(span) + 1) >=
                          max_page_to_fetch)
                      << "; total: " << model->page_table.size();
      }
    }
    ASSERT(pending * read_sz <= 4096 * 2);

    if (pending > 0) {
      fetcher.flush(pending - 1);
      RdmaFuture::spawn_future(R2_EXECUTOR, qp, 1);
      R2_PAUSE_AND_YIELD;
#if !COUNT_MISS
      data_transfered += 1;
#endif
    } else {
      ASSERT(false) << "non rdma to read from: " << std::get<0>(span) << " " << std::get<1>(span);
    }

    return Ok;
  }

  // return: which logic page, the index in the page
  Option<std::tuple<SearchCode, usize, usize>> search_within_pages(
    CBM* model,
    const std::pair<u64, u64>& span,
    const std::vector<u64> &addrs,
    const u64& key,
    char* buf)
  {
    bool whether_invalid = false;
    for (uint i = 0; i < std::get<1>(span) - std::get<0>(span) + 1; ++i) {
      char* page = buf + read_sz * i;
      Leaf* l_page = reinterpret_cast<Leaf*>(page);
      l_page->sanity_check();

      // check seq
      auto page_id = std::get<0>(span) + i;
      auto page_table_entry = model->lookup_page_entry(page_id).value();
      r2::compile_fence();

#if 1
      if (page_table_entry == INVALID_PAGE_ID ||
          SeqEncode::decode_seq(page_table_entry) != l_page->seq) {
        r2::compile_fence();

        //model->invalidate_page_entry(page_id);
        //return std::make_tuple(Invalid, 0u, 0u);
        whether_invalid = true;
      }
#endif

      auto idx = SC::Fetcher::lookup(l_page, key);
      if (idx) {
        return std::make_tuple(Ok,
                               static_cast<usize>(i + std::get<0>(span)),
                               static_cast<usize>(idx.value()));
      }
    }
    if (whether_invalid) {
      return std::make_tuple(Invalid, 0u, 0u);
    }
    return {};
  }
};

}
