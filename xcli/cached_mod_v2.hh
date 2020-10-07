#pragma once

namespace fstore {

class CacheKeyEncoder
{
public:
  static u64 encode_key(const usize& model_id, const usize& page_entry)
  {
    ASSERT(page_entry < 4096);
    return static_cast<u64>(model_id) * 4096 + page_entry;
  }

  static u64 decode_model_id(const u64& val) { return val / 4096; }

  static u64 decode_page_entry(const u64& val) { return val % 4096; }
};
class XCachedClientV2
{
public:
  LRModel<> dispatcher;
  usize num_total_model;
  usize max_addr;

  usize cur_cached = 0;

  // cached informs
  std::vector<SBM*> cached_models;
  std::vector<u64>   ext_addrs;
  std::vector<usize> ext_size;

  usize max_cached_tt_entry;
  usize cur_cached_tt = 0;

  u64 base_addr; // RDMA-base addr, for translating remote virtual address to
                 // RDMA offset
  u64 submodels_addr; // submodel-base addr

  u64 page_start;

  RCQP* qp = nullptr;

  static XCachedClientV2 *create(int table_id, usize max_cached_num, RPC &rpc, const Addr &server_addr, u64 page_start, R2_ASYNC) {
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
    //LOG(4) << "max cached num :" << max_cached_num;

    // parse the reply
    ModelMeta meta = Marshal<ModelMeta>::extract(reply_buf);
    LOG(0) << "sanity check meta at client : " << meta.dispatcher_sz << " "
           << meta.submodel_buf_addr << " " << meta.num_submodel << " "
           << meta.max_addr;

    return new XCachedClientV2(
      std::string(reply_buf + sizeof(ModelMeta), meta.dispatcher_sz),
      meta.num_submodel,
      max_cached_num,
      meta.max_addr,
      meta.base_addr,
      meta.submodel_buf_addr,
      page_start);
  }

  XCachedClientV2(const std::string& d,
                  usize total,
                  int max_cached_num,
                  usize max_addr,
                  u64 b,
                  u64 s,
                  u64 page_start)
    : num_total_model(total)
    , max_addr(max_addr)
    , base_addr(b)
    , submodels_addr(s)
    , page_start(page_start)
  {
    dispatcher.from_serialize(d);

    //LOG(4) << "init with page start :" << page_start;
    SBM temp;
    auto model_mem = num_total_model * temp.model_sz();
    if (model_mem < max_cached_num) {
      // init model
      for(uint i = 0;i < num_total_model;++i) {
        cached_models.push_back(nullptr);
        ext_addrs.push_back(0);
        ext_size.push_back(0);
      }

      max_cached_tt_entry = (max_cached_num - model_mem) / sizeof(u64);
    } else {
      max_cached_tt_entry = 0;
      auto cached_model_m = max_cached_num / temp.model_sz();
      for(uint i = 0;i < cached_model_m;++i) {
        cached_models.push_back(nullptr);
        ext_addrs.push_back(0);
        ext_size.push_back(0);
      }
    }
    LOG(0) << "max cached tt entry: " << max_cached_tt_entry << "; model mem: " << model_mem;
  }

  inline u64 virt_to_rdma(const u64& v) { return v - base_addr; }

  void fill_all_submodels(char* local_buf, R2_ASYNC)
  {
    SBM temp;
    for (uint i = 0; i < cached_models.size(); ++i) {
      this->get_model(i, temp, local_buf, R2_ASYNC_WAIT);
    }
    //LOG(4) << "after fill model: " << cur_cached_tt;
  }

  // model, ext
  std::pair<SBM *,u64> get_model(const usize &idx, SBM &temp, char *local_buf,R2_ASYNC) {
    if (idx < cached_models.size() && cached_models[idx] != nullptr) {
      return std::make_pair(cached_models[idx], ext_addrs[idx]);
    }
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
    auto sbm = new SBM();
    Serializer::extract_submodel_to(local_buf, *sbm);
    auto ret = this->model_cache.put(idx, sbm);
    if (ret) {
      delete ret.value();
    }
    return sbm;
#else
    Option<u64> ext = {};
    Serializer::extract_submodel_to(local_buf, temp ,ext);
    r2::compile_fence();
    //ASSERT(!ext) << "ext not supported in cached mod yet" << " for model: " << idx << " " << temp.page_table.size() <<  " " << ext.value();
    if (ext && this->cur_cached_tt + temp.page_table.size() < max_cached_tt_entry) {
      this->cur_cached_tt += temp.page_table.size();
#if 1
      op.set_read()
        .set_remote_addr(this->virt_to_rdma(ext.value()))
        .set_payload(local_buf, temp.page_table.size() * sizeof(u64));
      auto res = op.execute(R2_ASYNC_WAIT);
      ASSERT(std::get<0>(res) == SUCC);
      for (uint i = 0; i < temp.page_table.size(); ++i) {
        auto addr = Marshal<u64>::extract_with_inc(local_buf);
        temp.page_table[i] = addr;
      }
#endif
      if (idx < ext_addrs.size()) {
        ext_addrs[idx] = ext.value();
        ext_size[idx] = temp.page_table.size();
      }
    } else {
      //ASSERT(false);
      if (idx < ext_addrs.size()) {
        ext_addrs[idx] = ext.value();
        ext_size[idx] = temp.page_table.size();
      }

      temp.page_table.resize(0);
    }

    if (idx < cached_models.size()) {
#if 0
      if (ext) {
        temp.page_table.resize(0);
      }
#endif
      auto sbm = new SBM(temp);
      cached_models[idx] = sbm;
      //sbm->page_table.resize(0);
      return std::make_pair(sbm,ext.value());
    } else {
      //ASSERT(false);
      temp.page_table.resize(0);
    }
#endif
#endif
    //ASSERT(false);
    //ASSERT(temp.page_table.size() == 0);
    return std::make_pair(&temp,ext.value());
  }

  SearchCode get_direct(const u64& key, char* local_buf, R2_ASYNC){
    auto s = this->select_submodel(key);
    //LOG(4) << "get key: "<< key << " using model: " << s;
    // auto model = this->get_submodel(s, local_buf, R2_ASYNC_WAIT);
    SBM temp;
#if 1
    auto model_res = this->get_model(s, temp, local_buf, R2_ASYNC_WAIT);
    auto model = std::get<0>(model_res);
    auto ext   = std::get<1>(model_res);
#else
    auto model = cached_models[s];
    ASSERT(model != nullptr);
    auto ext = 0;
#endif

    auto page_range = model->get_page_span(key);

    // fetch pages
    std::vector<u64> page_addrs;
    if (model->page_table.size() != 0) {
      //LOG(4) << "already cached!";
      // already cached !
      for(uint i = std::get<0>(page_range); i <= std::get<1>(page_range) ;++i) {
        page_addrs.push_back(model->lookup_page_entry(i).value());
      }
    } else {

      if (this->cur_cached_tt + model->page_table.size() < max_cached_tt_entry && this->cached_models[s] != nullptr) {

        // the model must have been cached
        ASSERT(model == this->cached_models[s])
          << "model: " << model << " " << &temp
          << " in cached_model: " << cached_models[s] << " for " << s;

        char* addr_buf = local_buf;
        // using RDMA to fetch
        ::r2::rdma::SROp op(this->qp);

        ASSERT(ext_size[s] != 0) << "ext failed: " << s << "; this->cur_cached_tt: " << this->cur_cached_tt;
        op.set_read()
          .set_remote_addr(this->virt_to_rdma(ext))
          .set_payload(addr_buf, ext_size[s] * sizeof(u64));
        auto res = op.execute(R2_ASYNC_WAIT);
        ASSERT(std::get<0>(res) == SUCC);

        // fill in the cache
        for (uint i = 0; i < ext_size[s]; ++i) {
          auto addr = Marshal<u64>::extract_with_inc(addr_buf);
          model->page_table.push_back(addr);
        }
        this->cur_cached_tt += ext_size[s];

        for (uint i = std::get<0>(page_range); i <= std::get<1>(page_range);
             ++i) {
          page_addrs.push_back(model->lookup_page_entry(i).value());
        }
      } else {
        //LOG(4) << "on demand"; sleep(1);
        // on-demand fetch
        char* addr_buf = local_buf;
        ::r2::rdma::SROp op(this->qp);

        auto ps = std::get<0>(page_range);
        auto page_end   = std::get<1>(page_range);
        op.set_read()
          .set_remote_addr(this->virt_to_rdma(ext + ps * sizeof(u64)))
          .set_payload(addr_buf, (page_end - ps + 1) * sizeof(u64));
        auto res = op.execute(R2_ASYNC_WAIT);
        ASSERT(std::get<0>(res) == SUCC);

        for(uint i = ps; i <= page_end;++i) {
          auto addr = Marshal<u64>::extract_with_inc(addr_buf);
          page_addrs.push_back(addr);
        }
      }
    }
    //LOG(4) << "page addrs add: " << page_addrs.size();

    // then is the same as the previous models
    auto res =
      this->fetch_pages(s, page_addrs, page_range, local_buf, R2_ASYNC_WAIT);
    if (res != Ok) {
      if (res == Fallback) {
        // LOG(4) << "fallback in fetch pageS !"; sleep(1);
      }
      return res;
    }

    //LOG(4) << "fetch page done";
    auto ret = search_within_pages(model, page_range, key, local_buf);
    if (ret) {
      auto code = std::get<0>(ret.value());
      if (code != Ok) {
        if (code == Fallback) {
          // LOG(4) << "fallback in search within pages"; sleep(1);
        }
        return code;
      }
      // found
      auto page_id = std::get<1>(ret.value());
      auto idx = std::get<2>(ret.value());

      auto val_addr = this->page_to_rdma(SeqEncode::decode_id(page_addrs[page_id])) +
        Leaf::value_offset(idx);

      // issue the RDMA
      ::r2::rdma::SROp op(qp);
      op.set_read().set_remote_addr(val_addr).set_payload(
        local_buf, ValType::get_payload());
      auto res = op.execute(R2_ASYNC_WAIT);
      ASSERT(std::get<0>(res) == SUCC);
      data_transfered += 1;
      return Ok;

    } else {
      return None;
    }
    return Err;
  }

  const usize read_sz = Leaf::value_offset(0);
  //const usize read_sz = sizeof(Leaf);

  SearchCode fetch_pages(usize model_id,
                         const std::vector<u64> &page_addrs,
                         const std::pair<u64, u64>& span,
                         char* buf,
                         R2_ASYNC)
  {

    if ((std::get<1>(span) - std::get<0>(span) + 1) >= c_max_page_to_fetch) {
      return Unsafe;
    }

    ASSERT(page_addrs.size() != 0);
    ::fstore::X::RemoteFetcher<c_max_page_to_fetch> fetcher(qp, R2_COR_ID());

    int pending = 0;
    for (uint i = 0;i < page_addrs.size();++i) {
      auto e = page_addrs[i];
      auto page_addr =
        this->page_to_rdma(SeqEncode::decode_id(e));
      //LOG(4) << "read page addr: " << page_addr << " with page: " << SeqEncode::decode_id(e);

      pending = fetcher.add(
                            page_addr, read_sz, buf + (i) * read_sz);
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
    SBM* model,
    const std::pair<u64, u64>& span,
    const u64& key,
    char* buf)
  {
    for (uint i = 0; i < std::get<1>(span) - std::get<0>(span) + 1; ++i) {
      char* page = buf + read_sz * i;
      Leaf* l_page = reinterpret_cast<Leaf*>(page);
      l_page->sanity_check();

      //LOG(4) << "l page num keys: " << l_page->num_keys;
      // check seq
      auto page_id = std::get<0>(span) + i;

      auto idx = SC::Fetcher::lookup(l_page, key);
      if (idx) {
        return std::make_tuple(Ok,
                               static_cast<usize>(i),
                               static_cast<usize>(idx.value()));
      }
    }
    //LOG(4) << "not found!";
    return {};
  }


  usize
  select_submodel(const u64& key)
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

  inline u64 page_to_rdma(const u64& v)
  {
    //LOG(4) << "page to rdma start: " << page_start;
    return sizeof(Leaf) * v + page_start;
  }
};
}
