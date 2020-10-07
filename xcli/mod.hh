#pragma once

#include "./bootstrap.hh"
#include "./page_fetcher.hpp"

#include "../cli/fclient.hpp"

#define IDEAL 0

namespace fstore {

extern __thread u64 data_transfered;

constexpr usize max_page_to_fetch = 16;

class XCacheClient
{
public:

  // XCache core
  std::shared_ptr<XDirectTopLayer> core;

  // Two communication channel
  RPC* rpc;
  RCQP* qp;
  const u64 page_start;
  const u64 rdma_base;

  bool verbose = false;

  inline u64 page_to_rdma(const u64& v)
  {
    //LOG(4) << "page to rdma: " << v;
    auto res =  sizeof(Leaf) * v + page_start;
    ASSERT(v < 100000000u) << " invalid page id u: " << v;
    //LOG(4) << "converted res: " << res  << " " << page_start;
    return res;
  }

  inline u64 virt_to_rdma(const u64 &a) {
    return a - rdma_base;
  }

  XCacheClient(std::shared_ptr<XDirectTopLayer> core,
               RPC* rpc,
               RCQP* qp,
               const u64 base, const u64 rdma_base = 0)
    : core(core)
    , rpc(rpc)
    , qp(qp)
    , page_start(base),
      rdma_base(rdma_base)
  {}

  void debug_get(const u64 &key,char *local_buf, R2_ASYNC) {
    //LOG(4) << "debug get key: "<< key;
    auto s = core->select_submodel(key);
    auto model_to_predict = core->submodels[s];

    //LOG(4) << "select model: " << s;
    auto page_range = model_to_predict->get_page_span(key);
    LOG(4) << "page range: "<< std::get<0>(page_range) << " " << std::get<1>(page_range) << "; min max error: "
           << model_to_predict->min_error << " " << model_to_predict->max_error;

    auto predict = model_to_predict->get_predict(key);
    LOG(4) << "Get predict: " << predict;

    auto seq = model_to_predict->seq;

    auto res = this->fetch_pages(key,
      s, seq , model_to_predict, page_range, local_buf, R2_ASYNC_WAIT);
    if (res != Ok) {
      if (res == Fallback) {
        // LOG(4) << "fallback in fetch pageS !"; sleep(1);
      }
    }

    Leaf *l = (Leaf *)local_buf;
    for (uint i = 0;i < l->num_keys;++i) {
      LOG(4) << "check leaf key " << i << " : " << l->keys[i];
    }
  }

  SearchCode get_direct(const u64& key, char* local_buf, R2_ASYNC)
  {
    // first find the corresponding pages
    auto s = core->select_submodel(key);
    auto model_to_predict = core->submodels[s];
    //LOG(4) << "use model: " << s; sleep(1);

    auto seq = model_to_predict->seq;
    r2::compile_fence();
    if (unlikely(seq == INVALID_SEQ))
    {
      return  Fallback;
      //return Ok;
      //return Unsafe; // model is being concurrently updated
    }

    auto page_range = model_to_predict->get_page_span(key);
    // LOG(4) << "key: " << key << " use " << core->select_submodel(key) << "
    // for prediction";

    // LOG(4) << "Start to fetch page: "<< std::get<0>(page_range) << " " <<
    // std::get<1>(page_range); LOG(4) << "check model: " <<
    // model_to_predict->min_error << " " << model_to_predict->max_error;
    // 1. fetch pages in the local buf
    auto res = this->fetch_pages(key,
      s, seq, model_to_predict, page_range, local_buf, R2_ASYNC_WAIT);
    if (res != Ok) {
      if (res == Fallback) {
        //LOG(4) << "fallback in fetch pageS !"; sleep(1);
      }
      return res;
    }

    // 2. search
    auto ret =
      search_within_pages(model_to_predict, page_range, key, local_buf);
    if (ret) {
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

      // compute value address
      auto val_addr =
        this->page_to_rdma(model_to_predict->lookup_page_phy_addr(page_id)) +
        Leaf::value_offset(idx);

      // issue the RDMA
      ::r2::rdma::SROp op(qp);
      op.set_read().set_remote_addr(val_addr).set_payload(
        local_buf, ValType::get_payload());
      auto res = op.execute(R2_ASYNC_WAIT);
      ASSERT(std::get<0>(res) == SUCC);
      //data_transfered += 1;

      return Ok;

    } else {
      return None;
    }
    return Err;
  }

  SearchCode get_direct_possible_non_exist(const u64& key, char* local_buf, R2_ASYNC) {
    // first find the corresponding pages
    auto s = core->select_submodel(key);
    auto model_to_predict = core->submodels[s];
    //LOG(4) << "use model: " << s; sleep(1);

    auto seq = model_to_predict->seq;
    r2::compile_fence();
    if (unlikely(seq == INVALID_SEQ))
    {
      return  Fallback;
      //return Ok;
      //return Unsafe; // model is being concurrently updated
    }

    auto page_range = model_to_predict->get_page_span(key);
    // LOG(4) << "key: " << key << " use " << core->select_submodel(key) << "
    // for prediction";

    // LOG(4) << "Start to fetch page: "<< std::get<0>(page_range) << " " <<
    // std::get<1>(page_range); LOG(4) << "check model: " <<
    // model_to_predict->min_error << " " << model_to_predict->max_error;
    // 1. fetch pages in the local buf
    auto res = this->fetch_pages(key,
      s, seq, model_to_predict, page_range, local_buf, R2_ASYNC_WAIT);
    if (res != Ok) {
      if (res == Fallback) {
        //LOG(4) << "fallback in fetch pageS !"; sleep(1);
      }
      return res;
    }

    // first search the first page
    Leaf *first_page = reinterpret_cast<Leaf *>(local_buf);
    first_page->sanity_check();
    if (unlikely( key < first_page->keys[0])) {
      //return Fallback;
      // must exist in the previous page
      //ASSERT(std::get<0>(page_range) > 0);
      if (std::get<0>(page_range) == 0) {
        return Fallback;
      }
      return fetch_value_in_one_page(model_to_predict, std::get<0>(page_range) - 1, key, local_buf, R2_ASYNC_WAIT);
    }

    auto ret = search_within_pages(model_to_predict, page_range, key, local_buf);
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

      // compute value address
      auto val_addr =
        this->page_to_rdma(model_to_predict->lookup_page_phy_addr(page_id)) +
        Leaf::value_offset(idx);

      // issue the RDMA
      ::r2::rdma::SROp op(qp);
      op.set_read().set_remote_addr(val_addr).set_payload(
        local_buf, ValType::get_payload());
      auto res = op.execute(R2_ASYNC_WAIT);
      ASSERT(std::get<0>(res) == SUCC);
      // data_transfered += 1;

      return Ok;
    } else {
      // must in the later page
      return Fallback;
      return fetch_value_in_one_page(model_to_predict,
                                     std::get<1>(page_range) + 1,
                                     key,
                                     local_buf,
                                     R2_ASYNC_WAIT);
    }
    return Err;
  }

  const usize read_sz = Leaf::value_offset(0);
  //const usize read_sz = sizeof(Leaf);

  SearchCode fetch_pages(const u64 &key,
                         usize model_id,
                         u64   model_seq,
                         std::shared_ptr<SBM>& model,
                         const std::pair<u64, u64>& span,
                         char* buf,
                         R2_ASYNC)
  {

    if ((std::get<1>(span) - std::get<0>(span) + 1) >= max_page_to_fetch) {
      return Unsafe;
      //return Fallback;
    }

    ::fstore::X::RemoteFetcher<max_page_to_fetch> fetcher(qp, R2_COR_ID());

    int pending = 0;
    for (uint p = std::get<0>(span); p <= std::get<1>(span); ++p) {
      auto page_table_entry_opt = model->lookup_page_entry(p);
      if (page_table_entry_opt) {

        auto page_table_entry = page_table_entry_opt.value();
        r2::compile_fence();
        if (page_table_entry == INVALID_PAGE_ID) {
          //LOG(4) << "fallback model: " << model_id << " at entry: !!!" << p; sleep(1);
#if IDEAL
          return Ok;
#else
          return Fallback;
#endif
        }
        auto id = SeqEncode::decode_id(page_table_entry);
#if 1
        if (id >= 100000000) {
          ASSERT(false);
          return Unsafe;
        }
#endif
        auto page_addr =
          this->page_to_rdma(id);

        pending = fetcher.add(
          page_addr, read_sz, buf + (p - std::get<0>(span)) * read_sz);
      } else {
        // model may not be ready
        return Fallback;
        if (model->seq != model_seq || p >= max_page_entries_to_serialize) {
          return Unsafe;
        }
        ASSERT(false) << "page span: " << p << " from: " << std::get<0>(span)
                      << " " << std::get<1>(span) << " for model: " << model_id
                      << " check sz: " << std::get<1>(span) - std::get<0>(span)
                      << "; unsafe? "
                      << ((std::get<1>(span) - std::get<0>(span) + 1) >=
                          max_page_to_fetch) << " " << model->page_table.size() << "; error key: " << key;
      }
#if IDEAL
      // only fetch the first page
      break;
#endif
    }
    ASSERT(pending * read_sz <= 4096 * 2);

    if (pending > 0) {
      fetcher.flush(pending - 1);
      RdmaFuture::spawn_future(R2_EXECUTOR, qp, 1);
      R2_PAUSE_AND_YIELD;
      //data_transfered += 1;
      data_transfered += pending * read_sz;
    } else {
      ASSERT(false) << "non rdma to read from: " << std::get<0>(span) << " " << std::get<1>(span);
    }

    return Ok;
  }

  SearchCode scan(const u64& key, const usize& num, char* local_buf, R2_ASYNC)
  {
    auto s = core->select_submodel(key);
    auto model_to_predict = core->submodels[s];
    //LOG(4) << "scan key: " << key;

    auto seq = model_to_predict->seq;
    r2::compile_fence();
    if (unlikely(seq == INVALID_SEQ)) {
      return Fallback;
      // return Ok;
      // return Unsafe; // model is being concurrently updated
    }

    auto page_range = model_to_predict->get_page_span(key);
    // LOG(4) << "key: " << key << " use " << core->select_submodel(key) << "
    // for prediction";

    // LOG(4) << "Start to fetch page: "<< std::get<0>(page_range) << " " <<
    // std::get<1>(page_range); LOG(4) << "check model: " <<
    // model_to_predict->min_error << " " << model_to_predict->max_error;
    // 1. fetch pages in the local buf
    auto res = this->fetch_pages(key,
      s, seq, model_to_predict, page_range, local_buf, R2_ASYNC_WAIT);
    if (res != Ok) {
      if (res == Fallback) {
        // LOG(4) << "fallback in fetch pageS !"; sleep(1);
      }
      return res;
    }
    auto ret = search_lower_bound_within_pages(model_to_predict, page_range, key, local_buf);
    if (ret) {
      auto code = std::get<0>(ret.value());
      if (code != Ok) {
        if (code == Fallback) {
          // LOG(4) << "fallback in search within pages"; sleep(1);
        }
        return code;
      }
      auto page_id = std::get<1>(ret.value());
      //LOG(4) << "scan start with page id: " << page_id << " ; total: " << model_to_predict->page_table.size();
      auto page_addr =
        this->page_to_rdma(model_to_predict->lookup_page_phy_addr(page_id));
      return this->scan_with_start(model_to_predict, local_buf, page_addr, num, R2_ASYNC_WAIT);
    }
    return Err;
  }

  SearchCode scan_with_start(std::shared_ptr<SBM>& model,
                       char* local_buf,
                       const u64 &start_addr,
                       const usize& num,
                       R2_ASYNC)
  {
    if (unlikely(start_addr == this->page_start)) {
      // invalid
      return Fallback;
    }
    usize read_num = 0;
    auto page_addr = start_addr;
    //LOG(4) << "start addr: " << start_addr;

    while (read_num < num) {
      //LOG(4) << "scan page addr: " << page_addr;
      ::r2::rdma::SROp op(qp);
      op.set_read().set_remote_addr(page_addr).set_payload(local_buf,
                                                           sizeof(Leaf));
      auto res = op.execute(R2_ASYNC_WAIT);
      ASSERT(std::get<0>(res) == SUCC);

      Leaf* l = (Leaf*)local_buf;
      ASSERT(l->sanity_check_me()) << "sanity check leaf error; " << " " << " cur read num: " << read_num
                                   << "; page_start: " << this->page_start << " " << page_addr << " sizeof leaf: " << sizeof(Leaf)
                                   << " total read num: " << num << " " << rdma_base;
      read_num += l->num_keys;

      if (l->right == nullptr) {
        break; // end
      }
      page_addr = this->virt_to_rdma((u64)(l->right));
    }
    ASSERT(read_num > 0);
    return Ok;
  }

  Option<std::pair<SearchCode, usize>> search_lower_bound_within_pages(
    std::shared_ptr<SBM>& model,
    const std::pair<u64, u64>& span,
    const u64& key,
    char* buf) {
        for (uint i = 0; i < std::get<1>(span) - std::get<0>(span) + 1; ++i) {
      char* page = buf + read_sz * i;
      Leaf* l_page = reinterpret_cast<Leaf*>(page);

      // check seq
      auto page_id = std::get<0>(span) + i;
      auto page_table_entry = model->lookup_page_entry(page_id).value();
      r2::compile_fence();
      if (page_table_entry == INVALID_PAGE_ID ||
          SeqEncode::decode_seq(page_table_entry) != l_page->seq) {
        r2::compile_fence();
        if (this->verbose) {
          LOG(4) << "invalid: " << (int)l_page->seq << " "
                 << (int)SeqEncode::decode_seq(page_table_entry)
                 << " with page_entry :" << page_table_entry << " " << page_id; // sleep(1);
        }

#if IDEAL
        //return std::make_pair(
        //          Ok, static_cast<usize>(i + std::get<0>(span)), static_cast<usize>(0));
        return std::make_pair(
                              Ok, static_cast<usize>(i + std::get<0>(span)));
#else
        model->invalidate_page_entry(page_id);
        return std::make_pair(Invalid, static_cast<usize>(0));
#endif
      }

      // search the page entries
      for(uint j = 0;j < l_page->num_keys;++j) {
        if (l_page->keys[i] == key || l_page->keys[i] > key) {
          return std::make_pair(Ok, static_cast<usize>(page_id));
        }
      }
    }
    return std::make_pair(Ok, static_cast<usize>(std::get<1>(span)));
    // return std::make_pair(Fallback, 0u);
  }

  // return: which logic page, the index in the page
  Option<std::tuple<SearchCode, usize, usize>> search_within_pages(
    std::shared_ptr<SBM>& model,
    const std::pair<u64, u64>& span,
    const u64& key,
    char* buf)
  {
    for (uint i = 0; i < std::get<1>(span) - std::get<0>(span) + 1; ++i) {
      char* page = buf + read_sz * i;
      Leaf* l_page = reinterpret_cast<Leaf*>(page);
      ASSERT(l_page->sanity_check_me()) << "in search to the " << i << " th page error";

      // check seq
      auto page_id = std::get<0>(span) + i;
      auto page_table_entry = model->lookup_page_entry(page_id).value();
      r2::compile_fence();
      if (page_table_entry == INVALID_PAGE_ID) {
        return std::make_tuple(Fallback, 0u, 0u);
      }
      if (SeqEncode::decode_seq(page_table_entry) != l_page->seq) {
        r2::compile_fence();
        if (this->verbose) {
          LOG(4) << "invalid: " << (int)l_page->seq << " "
                 << (int)SeqEncode::decode_seq(page_table_entry)
                 << " with page_entry :" << page_table_entry << " " << page_id; // sleep(1);
        }

#if IDEAL
        return std::make_tuple(
          Ok, static_cast<usize>(i + std::get<0>(span)), static_cast<usize>(0));
#else
        model->invalidate_page_entry(page_id);
        return std::make_tuple(Invalid, 0u, 0u);
#endif
      }

      auto idx = SC::Fetcher::lookup(l_page, key);
      if (idx) {
        return std::make_tuple(Ok,
                               static_cast<usize>(i + std::get<0>(span)),
                               static_cast<usize>(idx.value()));
      }
#if IDEAL
      // always return a valid pointer
      return std::make_tuple(Ok,
                             static_cast<usize>(i + std::get<0>(span)),
                             static_cast<usize>(0));

#endif
    }
    return {};
  }

  SearchCode fetch_value_in_one_page(std::shared_ptr<SBM> &model, const u64 &id, const u64 &key, char *buf, R2_ASYNC) {
    auto page_table_entry_opt = model->lookup_page_entry(id);
    if (page_table_entry_opt) {
      auto page_entry = page_table_entry_opt.value();
      if (page_entry == INVALID_PAGE_ID) {
        return Fallback;
      }
      auto page_addr =
        this->page_to_rdma(SeqEncode::decode_id(page_entry));

      ::r2::rdma::SROp op(qp);
      op.set_read().set_remote_addr(page_addr).set_payload(buf,
                                                           read_sz);
      auto res = op.execute(R2_ASYNC_WAIT);
      ASSERT(std::get<0>(res) == SUCC);
      Leaf* l = (Leaf*)buf;

      ASSERT(l->sanity_check_me());

      if (SeqEncode::decode_seq(page_entry) != l->seq) {
        //model->invalidate_page_entry(id);
        return Invalid;
      }

      auto idx = SC::Fetcher::lookup(l, key);
      if (idx) {
        auto val_addr = page_addr + Leaf::value_offset(idx.value());
        ::r2::rdma::SROp op(qp);
        op.set_read().set_remote_addr(val_addr).set_payload(
          buf, ValType::get_payload());
        auto res = op.execute(R2_ASYNC_WAIT);
        ASSERT(std::get<0>(res) == SUCC);

        return Ok;

      } else {
        return None;
      }

    } else {
      return Fallback;
    }
    return Fallback;
  }
};

}
