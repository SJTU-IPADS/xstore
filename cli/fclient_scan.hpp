#pragma once

#include "fclient.hpp"

namespace fstore {

class FClientScan : public FClient
{
public:
  using FClient::FClient;

  inline void fetch_seek_page(const u64& page_num,
                              const u64& batch,
                              char* buf,
                              R2_ASYNC)
  {
    auto val_addr = page_addr + sizeof(Leaf) * sc->mp->mapped_page_id(page_num) +
                    Leaf::value_offset(0);
    auto res =
      qp->send({ .op = IBV_WR_RDMA_READ,
                 .flags = IBV_SEND_SIGNALED,
                 // TODO: link len with batch
                 .len = sizeof(ValType),
                 .wr_id = R2_COR_ID() },
               { .local_buf = buf, .remote_addr = val_addr, .imm_data = 0 });
    RdmaFuture::spawn_future(R2_EXECUTOR, qp, 1);
    R2_PAUSE_AND_YIELD;
    return;
  }

  inline void fetch_value_pages(const std::pair<u64, u64>& span,
                                char* buf,
                                R2_ASYNC)
  {
    //    auto size = sizeof(Leaf) - Leaf::value_offset(0);
    auto size = sizeof(ValType::data) * IM;

    u64 page_start(span.first), page_end(span.second);
    ASSERT(page_end - page_start + 1 < max_page_to_fetch)
      << "exceed max number of page to fetch " << page_num(span);
    RemoteFetcher<max_page_to_fetch> fetcher(qp, R2_COR_ID());
    ASSERT(max_page_to_fetch * sizeof(Leaf) <= 409600);

    int pending = 0;
    for (uint i(page_start); i <= page_end; ++i) {
      pending = fetcher.add(page_addr + sizeof(Leaf) * sc->mp->mapped_page_id(i),
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
  }

  inline bool iterative_seek(const u64& key,
                             char* local_buf,
                             const u64& batch,
                             R2_ASYNC)
  {
    auto predict = this->get_predict(key);
    {
      auto span = get_page_span(predict);
      // TODO: seems we donot need this
    }
  }

  inline Option<u64> seek_with_predict(const Predicts& p,
                                       const u64& key,
                                       char* local_buf,
                                       R2_ASYNC)
  {
    return {};
  }
  /*!
    Return a page which has the specific entries user requests
    \param num: the number of keys to fetched by the seek
    \ret: the logic page id
   */
  inline Option<u64> seek(const u64& key,
                          char* local_buf,
                          const u64& batch,
                          R2_ASYNC)
  {
    auto predicts = this->get_seek_predict(key);
    auto span = get_page_span(predicts);
    this->fetch_pages(span, local_buf, R2_ASYNC_WAIT);

#if 0    
    auto res = search_within_fetched_page(key, local_buf, span);
    if (res) {
      return res;
    } else
      return Option<u64>(73);
#endif
    // then, we search the pages
    char* page = local_buf;

    const u64 page_start(span.first), page_end(span.second);
    auto size = Leaf::value_offset(0);

    for (auto start(page_start); start < page_end; page += size, start += 1) {
      Leaf* l_page = reinterpret_cast<Leaf*>(page);
      l_page->sanity_check();

      // search within a leaf page
      for (uint i = 0; i < l_page->num_keys - 1; ++i) {
        if (l_page->keys[i] <= key && l_page->keys[i + 1] >= key) {
          // find one
          return Option<u64>(start);
        }
      }
      // check the last key
      if (key <= l_page->end_key()) {
        // find one
        return Option<u64>(start);
      }
    }

    // the we check the end page
    auto page_num = span.second;
    Leaf* l_page = reinterpret_cast<Leaf*>(page);
    //    l_page->sanity_check();

    for (int i = 0; i < l_page->num_keys - 1; ++i) {
      if (l_page->keys[i] <= key && l_page->keys[i + 1] >= key) {
        // find one
        return Option<u64>(page_end);
      }
    }
    if (l_page->end_key() >= key) {
      return Option<u64>(page_end);
    }
#if 0 // sanity check, the key must belong to the next
    fetch_one_page(page_end + 1, page, R2_ASYNC_WAIT);
    r2::compile_fence();
    l_page->sanity_check();
    if (l_page->start_key() >= key)
      return Option<u64>(page_end + 1);
#endif
    return Option<u64>(page_end + 1);
  }

private:
  /*!
    Only used for debug.
   */
  static void print_page_ranges(const std::pair<u64, u64>& span, char* buf)
  {
    char* cur_page = buf;
    for (auto start = span.first; start <= span.second;
         start += 1, cur_page += Leaf::value_offset(0)) {
      Leaf* l_page = reinterpret_cast<Leaf*>(cur_page);
      LOG(4) << "page #" << start << " range: " << l_page->start_key() << " ~ "
             << l_page->end_key();
    }
  }
};

} // namespace fstore
