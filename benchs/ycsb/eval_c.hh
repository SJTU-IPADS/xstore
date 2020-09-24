#pragma once

// db schema
#include "./schema.hh"

#include "../../deps/r2/src/rdma/async_op.hh"
using namespace r2::rdma;

#include "../../xcomm/src/batch_rw_op.hh"

namespace xstore {
extern std::unique_ptr<XCache> cache;
extern std::vector<XCacheTT> tts;

using namespace xcomm;

auto core_eval(const XKey &key, const Arc<RC> &rc, char *my_buf, R2_ASYNC)
    -> ValType {
  // evaluate a get() req
  const usize read_sz = DBTree::Leaf::value_start_offset();

  // 1. predict
  auto m = cache->select_sec_model(key);
  auto range = cache->get_predict_range(key);

  auto ns = std::max(std::get<0>(range) / 16, 0);
  auto ne =
      std::min(std::get<1>(range) / 16, static_cast<int>(tts[m].size() - 1));

  BatchOp<16> reqs;
  for (auto p = ns; p <= ne; ++p) {
    reqs.emplace();
    reqs.get_cur_op()
        .set_read()
        .set_rdma_rbuf((const u64 *)(tts.at(m)[p]), rc->remote_mr.value().key)
        .set_payload(my_buf + read_sz * (p - ns), read_sz,
                     rc->local_mr.value().lkey);
  }

  auto ret = reqs.execute_async(rc, R2_ASYNC_WAIT);
  ASSERT(ret == ::rdmaio::IOCode::Ok);

  // then find the value
  for (auto p = ns; p <= ne; ++p) {
    DBTree::Leaf *node = reinterpret_cast<DBTree::Leaf *> (my_buf + read_sz * (p - ns));
    auto idx = node->search(key);
    if (idx) {
      // fetch the value
      AsyncOp<1> op;
      op.set_read()
        .set_rdma_rbuf(tts.at(m)[p] + DBTree::Leaf::value_offset(idx.value()),
                         rc->remote_mr.value().key)
        .set_payload(my_buf, sizeof(ValType), rc->local_mr.value().lkey);
      ret = op.execute_async(rc, IBV_SEND_SIGNALED, R2_ASYNC_WAIT);
      ASSERT(ret == ::rdmaio::IOCode::Ok);

      return *((ValType *)my_buf);
    }
  }
  // failed to found
  ASSERT(false);
  ValType res;
  return res;
}

} // namespace xstore
