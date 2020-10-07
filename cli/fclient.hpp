#pragma once

#include "r2/src/allocator.hpp"
#include "r2/src/futures/rdma_future.hpp"
#include "r2/src/rdma/single_op.hpp"

#include "page_fetcher.hpp"

namespace fstore {

extern __thread u64 data_transfered;

// if is set to 1, then we check whether the fetched has been split, and
// fallback if necessary
#define CHECK_SPLIT 1

/*!
  Search status of the page
 */
enum SearchCode
{
  Ok = 0,       // we found the page
  Invalid,  // some page has been invalided
  Fallback, // fallback to RPC because we detect the invalid page in a fast path
  Unsafe,   // not safe
  None,      // not found
  Err,
  None_prev,
  None_next,
};

/*!
  FClient is the client library of fstore, using one-sided RDMA.
  Pre-requests:
  - 1. RCQP is connected.
  - 2. Thread local allocator is initialized.
  - 3. Remote smart cache is bootstraped using model fetcher.
*/
class FClient
{
public:
  static constexpr const int max_page_to_fetch = 18;
public:
  FClient(RCQP* qp, SC* remote_sc, u64 page_start, u64 page_area_sz)
    : sc(remote_sc)
    , page_addr(page_start)
    , page_area_sz(page_area_sz)
    , qp(qp)
  {}

  inline Option<u64> get_addr(const u64& key, RScheduler& coro, handler_t& h)
  {
    auto span = get_page_span(key);
    char* buf = (char*)AllocatorMaster<>::get_thread_allocator()->alloc(
      page_num(span) * sizeof(Leaf));
    auto ret = get_addr(key, buf, coro, h);
    AllocatorMaster<>::get_thread_allocator()->free(buf);
    return std::get<1>(ret);
  }

  /*!
This predits is a different version of predicts used in smart_cache.
The traditional smart_cache uses the bottom line model to predict the
position. However, this solution only works for situations when keys are
pretrained.
  So we use a more complicated predict, which uses 3 models to pick the final
postiion.
 */
  inline Predicts get_seek_predict(const u64& key)
  {
    auto model = sc->index->get_model(key);
    auto p0 = sc->index->predict_w_model(key, model);
#if 1 // below is the main algorithms
    auto p1 = sc->index->predict_w_model(key, model == 0 ? 0 : model - 1);

    // now we have three predicts, we expand this to the final predict
    if (sc->intersect(p0, p1)) {

      p0.start = std::min(p0.start, p1.start);
      p0.end = std::max(p0.end, p1.end);
    }
#endif
    return p0;
  }

  /*!
    Allow user to pass a local buffer to store the fetched pages.
    This call is dangerous because we donot do boundary checks.
    However, it is very important for high-performnace computing,
    since user-managed buffer has better cache locality.
  */
  // TODO: change this name to *get*
  using GetResult = std::pair<SearchCode, u64>;
  inline GetResult get_addr(const u64& key,
                            char* local_buf,
                            RScheduler& coro,
                            handler_t& h)
  {
    auto predict = sc->get_predict(key);
    //    auto predict = get_seek_predict(key);
    return get_addr(key, predict, local_buf, coro, h);
  }

  inline GetResult get_addr(const u64& key,
                            const Predicts& predict,
                            char* local_buf,
                            RScheduler& coro,
                            handler_t& h)
  {
#if 0
    if (predict.end - predict.start <= 1) {
      // fast path, no need to fetch the entire page
      return get_addr_fast(key, predict, local_buf, coro, h);
    }
#endif

    auto span = get_page_span(predict);
    auto fetch_res = fetch_pages(span,local_buf,h,coro);
    if ( fetch_res != Ok) {
      //ASSERT(false) << fetch_res;
      return std::make_pair(fetch_res,0);
    }
    //return std::make_pair(Ok,0);

    auto res = search_within_fetched_page(key, local_buf, span);
    if (std::get<0>(res) == Ok) {
#if 1
      qp->send({ .op = IBV_WR_RDMA_READ,
                 .flags = IBV_SEND_SIGNALED,
                 .len = ValType::get_payload(),
                 .wr_id = coro.cur_id() },
               { .local_buf = local_buf,
                 .remote_addr = std::get<1>(res),
                 .imm_data = 0 });
      RdmaFuture::spawn_future(coro, qp, 1);
      data_transfered += ValType::get_payload();
      coro.pause(h);
#endif
      return std::make_pair(Ok, (Marshal<u64>::extract(local_buf)));
    } else {
      //auto ret_code = std::get<0>(res); // sanity check the ret code
      //ASSERT(ret_code == Ok) << "faild to get the value: " << ret_code;
    }
    return res;
  }

  using SeekResult = std::pair<SearchCode, std::pair<u64,u64>>; // ( search_code, (page_num, page_off))
  inline SeekResult seek(const u64& key,
                            char* local_buf,
                            R2_ASYNC) {
    //auto predict = get_seek_predict(key);
    auto predict = get_predict(key);
    auto span = get_page_span(predict);
    u64 page_start(span.first), page_end(span.second);
    if(page_end - page_start + 1 > max_page_to_fetch) {
      return std::make_pair(Unsafe,std::make_pair(0,0));
    }
    //LOG(4) << "seek key: " << key << " using model: " << sc->index->get_model(key);

    auto fetch_res = fetch_pages(span, local_buf, R2_ASYNC_WAIT);
    if (fetch_res != SearchCode::Ok)
    {
      //ASSERT(false);
      return std::make_pair(fetch_res,std::make_pair(0,0));
    }

    // the we return the page number
    char *page = local_buf;
    usize page_sz = Leaf::value_offset(0);

    auto start = std::get<0>(span); auto end = std::get<1>(span);
    for(auto s(start);s < end;++s, page += page_sz) {
      Leaf* l_page = reinterpret_cast<Leaf*>(page);
      l_page->sanity_check();

#if CHECK_SPLIT
      // then we check whether the fetched page is valid
      auto encoded_page_id = sc->mp->mapped_page_id(s);
      if (unlikely(SeqEncode::decode_seq(encoded_page_id) != (page_entry_t)(l_page->seq))) {

        // update the mega table to invalid the page
        //ASSERT(false) << "decoded seq: "<< SeqEncode::decode_seq(encoded_page_id) <<
        //          "compare id: " << (page_entry_t)(l_page->seq);
        //LOG(4) << "invalid entry: " << start;
        ASSERT((page_entry_t)(l_page->seq) > SeqEncode::decode_seq(encoded_page_id));
        sc->mp->invalid_page_entry(s);
        return std::make_pair(Invalid, std::make_pair(0,0));
      }
#endif

      // finally we search the lower bound
      for (uint i = 0; i < l_page->num_keys - 1; ++i) {
        if (l_page->keys[i] <= key && l_page->keys[i + 1] >= key) {
          // find one
          return std::make_pair(Ok,std::make_pair(s,i));
        }
      }
      if (key <= l_page->end_key()) {
        // find one at last
        return std::make_pair(Ok,std::make_pair(s,l_page->num_keys - 1));
      }
    }
    // finally we check the last page
    Leaf* l_page = reinterpret_cast<Leaf*>(page);
    l_page->sanity_check();

    {
      auto encoded_page_id = sc->mp->mapped_page_id(end);
      if (unlikely(SeqEncode::decode_seq(encoded_page_id) != (page_entry_t)(l_page->seq))) {

        // update the mega table to invalid the page
        //ASSERT(false) << "decoded seq: "<< SeqEncode::decode_seq(encoded_page_id) <<
        //          "compare id: " << (page_entry_t)(l_page->seq);
        //LOG(4) << "invalid entry: " << start;
        ASSERT((page_entry_t)(l_page->seq) > SeqEncode::decode_seq(encoded_page_id));
        sc->mp->invalid_page_entry(end);
        return std::make_pair(Invalid, std::make_pair(0,0));
      }
    }

    for (int i = 0; i < l_page->num_keys - 1; ++i) {
      if (l_page->keys[i] <= key && l_page->keys[i + 1] >= key) {
        // find one
        return std::make_pair(Ok,std::make_pair(end,i));
      }
    }
    if (l_page->end_key() >= key) {
      return std::make_pair(Ok,std::make_pair(end,l_page->num_keys - 1));
    }

    return std::make_pair(Ok,std::make_pair(end + 1,0));
  }


  inline u64 random_fetch_value(r2::util::FastRandom& rand,
                                char* buf,
                                u64 payload,
                                RScheduler& coro,
                                handler_t& h)
  {
    auto addr = rand.next() % (page_area_sz - payload) + page_addr;
    auto res =
      qp->send({ .op = IBV_WR_RDMA_READ,
                 .flags = IBV_SEND_SIGNALED,
                 .len = sizeof(u64),
                 .wr_id = coro.cur_id() },
               { .local_buf = buf, .remote_addr = addr, .imm_data = 0 });
    RdmaFuture::spawn_future(coro, qp, 1);

    coro.pause(h);

    return Marshal<u64>::extract(buf);
  }

  /*!
    If the number of predict is one, then we can use one RDMA roundtrip to
    fetch a small number of records
  */
  inline std::pair<SearchCode,u64> get_addr_fast(const u64& key,
                                   const Predicts& predict,
                                   char* local_buf,
                                   RScheduler& coro,
                                   handler_t& h)
  {
    /*
     * XD: seems that using the per-request allocator will has server
     performance problem. We should use a per client local buffer, otherwise
     cache is an important problem
    */
    // char *buf = (char
    // *)AllocatorMaster<>::get_thread_allocator()->alloc(sizeof(u64) * 2);

    /**
     * buffer to store the fetched back value.
     */
    char* buf = local_buf;
    /**
     * There is one request, so the future will wait for one response.
     * On the other hand, there is 2 doorbelled RDMA for this request, so the
     * QP will send *2* number.
     */
    auto page = sc->mp->decode_mega_to_entry(predict.start);
    auto page_id = SeqEncode::decode_id(
                                        sc->mp->mapped_page_id(page));
    if (page_id == 0) {
      // an invalided page
      return std::make_pair(Fallback,0);
    }
    auto off = sc->mp->get_offset(predict.start);

    auto res =
      qp->send({ .op = IBV_WR_RDMA_READ,
                 .flags = IBV_SEND_SIGNALED,
                 .len = ValType::get_payload(),
                 .wr_id = coro.cur_id() },
               { .local_buf = buf,
                 .remote_addr = page_addr +
                                sizeof(Leaf) * page_id +
                                Leaf::value_offset(off),
                 .imm_data = 0 });
    RdmaFuture::spawn_future(coro, qp, 1);

    coro.pause(h);

    return std::make_pair(Ok,Marshal<u64>::extract(buf));
  }

  /*!
    If the number of predict is one, then we can use one RDMA roundtrip to
    fetch a small number of records This method will verify that the kv is
    truly correct.
  */
  inline Option<u64> get_addr_fast_w_verify(const u64& key,
                                            const Predicts& predict,
                                            char* local_buf,
                                            RScheduler& coro,
                                            handler_t& h)
  {
    /*
     * XD: seems that using the per-request allocator will has server
     performance problem. We should use a per client local buffer, otherwise
     cache is an important problem
    */
    // char *buf = (char
    // *)AllocatorMaster<>::get_thread_allocator()->alloc(sizeof(u64) * 2);

    /**
     * buffer to store the fetched back value.
     */
    char* buf = local_buf;
    /**
     * There is one request, so the future will wait for one response.
     * On the other hand, there is 2 doorbelled RDMA for this request, so the
     * QP will send *2* number.
     */
    auto page = sc->mp->decode_mega_to_entry(predict.start);
    auto off = sc->mp->get_offset(predict.start);

    ASSERT(false) << "not implemented";
    return {};
  }

  inline std::pair<u64, u64> get_page_span(const Predicts& predict)
  {
    auto end = std::min<u64>(sc->mp->decode_mega_to_entry(predict.end),sc->mp->total_num() - 1);
    //auto end = sc->mp->decode_mega_to_entry(predict.end);
    return std::make_pair(
      sc->mp->decode_mega_to_entry(predict.start),
      end);
  }

  Predicts get_predict(const u64& key) { return sc->get_predict(key); }

  inline std::pair<u64, u64> get_page_span(const u64& key)
  {
    auto predict = sc->get_predict(key);
    return get_page_span(predict);
  }

  inline void fetch_one_page(const u64& page_id, char* buf, R2_ASYNC)
  {
    ASSERT(false) << "deprecated";
    auto remote_addr =
      page_addr + sizeof(Leaf) * sc->mp->mapped_page_id(page_id);
    ::r2::rdma::SROp op(qp);
    op.set_read()
      .set_remote_addr(remote_addr)
      .set_payload(buf, Leaf::value_offset(0));
    auto res = op.execute(R2_ASYNC_WAIT);
    ASSERT(std::get<0>(res) == SUCC);
  }

  inline bool scan_with_seek(const std::pair<u64,u64> &seek, int num, char *buf,R2_ASYNC) {

    auto size = Leaf::value_offset(IM) - Leaf::seq_offset();
    RemoteFetcher<max_page_to_fetch> fetcher(qp, R2_COR_ID());

    auto cur = std::get<0>(seek);
    auto off = std::get<1>(seek);

    int num_fetched = 0;
    int pending_rdma = 0;

    //LOG(4) << "scan at page: " << cur << " with offset: " << off; sleep(1);

    // first we fill the first page scan result
    if(cur >= sc->mp->all_ids_.size())
      return true;

    auto encoded_page_id = sc->mp->mapped_page_id(cur);
    if(unlikely(encoded_page_id == 0))
      return false;

    auto temp = SeqEncode::decode_sz(encoded_page_id);
    //ASSERT(temp < 16) << "invalid sz: " << temp;
    if (off < temp) {
      pending_rdma = fetcher.add(
        page_addr + sizeof(Leaf) * SeqEncode::decode_id(encoded_page_id) +
          Leaf::seq_offset(),
        // sizeof(Leaf), // size
        // sizeof(u64),
        // 1,
        // FLAGS_rdma_payload,
        (temp - off) * ValType::get_payload(),
        buf);
    }
    num_fetched += (temp - off);
    //LOG(4) << "first fetched";

    while(num_fetched < num) {
      cur += 1;

      if(cur >= sc->mp->all_ids_.size())
        break;

      auto encoded_page_id = sc->mp->mapped_page_id(cur);
      if(unlikely(encoded_page_id == 0))
        return false;

      auto sz = SeqEncode::decode_sz(encoded_page_id);
      pending_rdma = fetcher.add(
        page_addr + sizeof(Leaf) * SeqEncode::decode_id(encoded_page_id) +
          Leaf::seq_offset(),
        // sizeof(Leaf), // size
        // sizeof(u64),
        // 1,
        // FLAGS_rdma_payload,
        sz * ValType::get_payload(),
        buf + temp);
      temp += sz * ValType::get_payload(); // FIXME: current we donot handle
                                           // concurrency at this level
      num_fetched += sz;
    }

    // now issue the RDMA request
    if (pending_rdma > 0) {
      // LOG(4) << "flush pending: " << pending; sleep(1);
      fetcher.flush(pending_rdma - 1); // its index
      RdmaFuture::spawn_future(R2_EXECUTOR, qp, 1);
      R2_PAUSE_AND_YIELD;
    }

    return true;
  }

  inline bool fetch_value_pages(const std::pair<u64, u64>& span, char* buf, R2_ASYNC) {

    /*!
      can we store values in column arraies to accelerate scan over one column?
      YCSB in default use all columns
     */
    auto size = Leaf::value_offset(IM) - Leaf::seq_offset();

    RemoteFetcher<max_page_to_fetch> fetcher(qp, R2_COR_ID());

    u64 page_start(span.first), page_end(span.second);
    ASSERT(page_end - page_start + 1 < max_page_to_fetch);

    int pending = 0;
    for (uint i(page_start); i <= page_end; ++i) {
      if(i >= sc->mp->all_ids_.size())
        break;

      auto encoded_page_id = sc->mp->mapped_page_id(i);

      if (unlikely(SeqEncode::decode_id(encoded_page_id) == 0)) {
        return false;
      }
      pending = fetcher.add(page_addr + sizeof(Leaf) *
                            SeqEncode::decode_id(encoded_page_id) + Leaf::seq_offset(),
                            // sizeof(Leaf), // size
                            // sizeof(u64),
                            // 1,
                            // FLAGS_rdma_payload,
                            size,
                            buf + (i - page_start) * size);
    }
    if (pending > 0) {
      // LOG(4) << "flush pending: " << pending; sleep(1);
      fetcher.flush(pending - 1); // its index
      RdmaFuture::spawn_future(R2_EXECUTOR, qp, 1);
      R2_PAUSE_AND_YIELD;
    }

    return true;
  }

  // TODO: error handling
  /*!
  \ret: true - everything is ok
        false - some page has splited
  \note: this function does not invalid pages.
         invalidation will be checked after this call
   */
  inline SearchCode fetch_pages(const std::pair<u64, u64>& span, char* buf, R2_ASYNC)
  {
    //auto size = Leaf::value_offset(0) + IM * sizeof(u64);
    auto size = Leaf::value_offset(0);
    //auto size = sizeof(Leaf);

    u64 page_start(span.first), page_end(span.second);
    if(unlikely(page_end - page_start + 1 >= max_page_to_fetch))
      return Unsafe;
    data_transfered += ((page_end - page_start + 1) * size);
    ASSERT(page_end - page_start + 1 < max_page_to_fetch)
      << "exceed max number of page to fetch " << page_num(span)
      << "page range: " << page_start << " ~ " << page_end;
    // page_end = page_start;
#if 0
    uint pending = 0;
    for (uint i(page_start); i <= page_end; ++i) {
      auto res = qp->send(
        { .op = IBV_WR_RDMA_READ,
          .flags = IBV_SEND_SIGNALED,
          .len = size,
          //.len   = Leaf::value_offset(0),
          //.len = 1,
          .wr_id = R2_COR_ID() },
        { .local_buf = buf + (i - page_start) * size,
          .remote_addr = page_addr + sizeof(Leaf) * sc->mp->mapped_page_id(i),
          .imm_data = 0 });
      ASSERT(res == SUCC);
      pending += 1;

      if (unlikely(qp->progress_.pending_reqs() >= 64)) {
        RdmaFuture::spawn_future(R2_EXECUTOR, qp, pending);
        R2_PAUSE_AND_YIELD;
        pending = 0;
      }
    }
    RdmaFuture::spawn_future(R2_EXECUTOR, qp, pending);
    R2_PAUSE_AND_YIELD;
#else
    RemoteFetcher<max_page_to_fetch> fetcher(qp, R2_COR_ID());

    //LOG(4) << "seek key: " <<
    //      "page range: " << page_start << "; " << page_end << " -- total: " << sc->mp->total_num();

    //const auto end = std::min(page_end,sc->mp->total_num() - 1);
#if 0
    auto encoded_page_id = sc->mp->mapped_page_id(page_start);
    auto remote_addr =
      page_addr + sizeof(Leaf) * SeqEncode::decode_id(encoded_page_id);
    auto res = qp->send(
           { .op = IBV_WR_RDMA_READ,
             .flags = IBV_SEND_SIGNALED,
             .len = Leaf::value_offset(0),
             .wr_id = R2_COR_ID() },
           { .local_buf = buf + (page_start - page_start) * sizeof(Leaf),
             .remote_addr = remote_addr,
             .imm_data = 0 });
    RdmaFuture::spawn_future(R2_EXECUTOR, qp, 1);
    R2_PAUSE_AND_YIELD;

    return Ok;
#endif
    int pending = 0;
    for (uint i(page_start); i <= page_end; ++i) {
      auto encoded_page_id = sc->mp->mapped_page_id(i);

      if (unlikely(SeqEncode::decode_id(encoded_page_id) == 0)) {
        //LOG(4) << "fetch page failed with entry: " << i << "; using sc: " << sc;
        return Fallback;
      }
      //LOG(4) << "fetch with page id: "
      //<< SeqEncode::decode_id(encoded_page_id) << " with entry: " << i
      //<< "with model sz: " << sc->index->rmi.second_stage->models.size()
      //<< "; entries: " << sc->mp->total_num();

      //ASSERT(SeqEncode::decode_id(encoded_page_id) <= sc->mp->total_num())
      //<< "sanity check 0' entry: " << sc->mp->mapped_page_id(0);
      pending = fetcher.add(
                            page_addr + sizeof(Leaf) * SeqEncode::decode_id(encoded_page_id),
        // sizeof(Leaf), // size
        // sizeof(u64),
        // 1,
        // FLAGS_rdma_payload,
        size,
        buf + (i - page_start) * size);
#if 0
      if(unlikely(qp->progress_.pending_reqs() >= 64)) {
        auto ret = fetcher.flush(pending - 1); // its index
        ASSERT(ret == SUCC);
        ASSERT(pending > 0);

        RdmaFuture::spawn_future(coro,qp,pending);
        coro.pause(h);
        pending = 0;
      }
#endif
    }
    //LOG(4) << "done one";
    if (pending > 0) {
      // LOG(4) << "flush pending: " << pending; sleep(1);
      fetcher.flush(pending - 1); // its index
      RdmaFuture::spawn_future(R2_EXECUTOR, qp, 1);
      R2_PAUSE_AND_YIELD;
    }
#endif

    return Ok;
  }

  inline GetResult random_fetch_page(r2::util::FastRandom& rand,
                                const std::pair<u64, u64>& span,
                                char* buf,
                                RScheduler& coro,
                                handler_t& h)
  {
    const u64 page_start(span.first), page_end(span.second);
    uint pending = 0;

    for (uint i(page_start); i <= page_end; ++i) {
      auto res =
        qp->send({ .op = IBV_WR_RDMA_READ,
                   .flags = IBV_SEND_SIGNALED,
                   .len = Leaf::value_offset(0),
                   .wr_id = coro.cur_id() },
                 { .local_buf = buf + (i - page_start) * sizeof(Leaf),
                   //.remote_addr = page_addr + rand.next() % page_area_sz,
                   .remote_addr = rand.next() %  (1 * GB),
                   .imm_data = 0 });
      ASSERT(res == SUCC);
      pending += 1;

      if (unlikely(qp->progress_.pending_reqs() >= 16)) {
        RdmaFuture::spawn_future(coro, qp, pending);
        coro.pause(h);
        pending = 0;
      }
      break;
    }
    RdmaFuture::spawn_future(coro, qp, pending);
    coro.pause(h);
    return std::make_pair(Ok,0);
  }

  using SearchResult = std::pair<SearchCode, u64>;
  inline SearchResult search_within_fetched_page(
    const u64& key,
    char* page_buf,
    const std::pair<u64, u64>& span)
  {
    const u64 page_start(span.first), page_end(span.second);
    auto page = page_buf;
    //auto size = Leaf::value_offset(0) + IM * sizeof(u64);
    auto size = Leaf::value_offset(0);
    //auto size = sizeof(Leaf);

    for (auto start(page_start); start <= page_end; page += size, start += 1) {
      //LOG(4) << "sanity check page: " << start;

      // first we sanity check the page
      Leaf* l_page = reinterpret_cast<Leaf*>(page);
      l_page->sanity_check();

#if CHECK_SPLIT
      // then we check whether the fetched page is valid
      auto encoded_page_id = sc->mp->mapped_page_id(start);
      if (unlikely(SeqEncode::decode_seq(encoded_page_id) != (page_entry_t)(l_page->seq))) {
        // update the mega table to invalid the page

        //ASSERT(false);
          sc->mp->invalid_page_entry(start);
        return std::make_pair(Invalid, 0);
      }
#endif
      // sanity checks
      if (0) {
        page_entry_t sz = SeqEncode::decode_sz(encoded_page_id);
        auto check_sz = static_cast<page_entry_t>(l_page->num_keys);
        ASSERT(check_sz >= sz); // the checked sz can be larger due to insertions
        // but it cannot become smaller
      }

      auto idx = SC::Fetcher::lookup(l_page, key);
      if (idx) {
        return std::make_pair(
          Ok,
          page_addr + sizeof(Leaf) * SeqEncode::decode_id(encoded_page_id) +
            Leaf::value_offset(idx.value()));
      }
    }
    return std::make_pair(None,
                          page_addr +
                            sizeof(Leaf) * SeqEncode::decode_id(page_start) +
                            Leaf::value_offset(0));
  }

  static u64 page_num(const std::pair<u64, u64>& span)
  {
    return span.second - span.first + 1;
  }

protected:
  SC* sc = nullptr;
  RCQP* qp = nullptr;
  u64 page_addr = 0;
  u64 page_area_sz = 0;
};

} // fstore
