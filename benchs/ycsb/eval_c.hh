#pragma once

#include <gflags/gflags.h>

// db schema
#include "./schema.hh"

#include "../../deps/r2/src/rdma/async_op.hh"
using namespace r2::rdma;

#include "../../xcomm/src/batch_rw_op.hh"

#include "../../xcomm/src/rpc/mod.hh"
#include "../../xcomm/src/transport/rdma_ud_t.hh"

#include "../../xutils/cdf.hh"

namespace xstore {

using namespace xstore::rpc;
using namespace xstore::transport;
using namespace xstore::util;

// prepare the sender transport
using SendTrait = UDTransport;
using RecvTrait = UDRecvTransport<2048>;
using SManager = UDSessionManager<2048>;

extern std::unique_ptr<XCache> cache;
extern std::vector<XCacheTT> tts;

__thread usize worker_id;

DEFINE_int32(len, 8, "average length of the value");
DEFINE_int32(client_name, 0, "Unique client name (in int)");

using namespace xcomm;

using RPC = RPCCore<SendTrait, RecvTrait, SManager>;

CDF<usize> error_cdf("");

auto eval_w_rpc(const XKey &key, RPC &rpc, UDTransport &sender, R2_ASYNC) -> ValType {
  char send_buf[64];
  char reply_buf[sizeof(ValType)];

  RPCOp op;
  op.set_msg(MemBlock(send_buf, 64))
      .set_req()
      .set_rpc_id(GET)
      .set_corid(R2_COR_ID())
      .add_one_reply(rpc.reply_station, {.mem_ptr = reply_buf, .sz = sizeof(ValType)})
      .add_arg<XKey>(key);
  ASSERT(rpc.reply_station.cor_ready(R2_COR_ID()) == false);
  auto ret = op.execute_w_key(&sender, 0);
  ASSERT(ret == IOCode::Ok);

  // yield the coroutine to wait for reply
  R2_PAUSE_AND_YIELD;

  // check the rest
  return *(reinterpret_cast<ValType *>(reply_buf));
}

auto core_eval(const XKey &key, const Arc<RC> &rc, RPC &rpc,
               UDTransport &sender, char *my_buf,
               ::xstore::bench::Statics &s,
               R2_ASYNC) -> ValType {
  // evaluate a get() req
  const usize read_sz = DBTree::Leaf::value_start_offset();

  // 1. predict
  auto m = cache->select_sec_model(key);
  auto range = cache->get_predict_range(key);

  auto ns = std::max<int>(std::get<0>(range) / kNPageKey, 0);
  //auto ne =
  //std::min(std::get<1>(range), static_cast<int>(tts[m].size() - 1)) / 16;
  auto ne = std::min<int>(static_cast<int>(tts[m].size() - 1),
                          std::get<1>(range) / kNPageKey);
  // record statics
  if (FLAGS_client_name == 1 && worker_id == 0) {
    error_cdf.insert(std::get<1>(range) - std::get<0>(range));
  }

  const usize kMaxBatch = 10;
  BatchOp<kMaxBatch> reqs;
  //LOG(4) << "get key: "<< key; sleep(1);

  if (unlikely(ne - ns + 1 > kMaxBatch)) {
    // unsafe case
    s.increment_gap_1(1);
    return eval_w_rpc(key, rpc, sender, R2_ASYNC_WAIT);
  }

  for (auto p = ns; p <= ne; ++p) {
    reqs.emplace();
    reqs.get_cur_op()
        .set_read()
        .set_rdma_rbuf((const u64 *)(tts.at(m).get_wo_incar(p)), rc->remote_mr.value().key)
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
          .set_rdma_rbuf(tts.at(m).get_wo_incar(p) + DBTree::Leaf::value_offset(idx.value()),
                         rc->remote_mr.value().key)
        .set_payload(my_buf, std::max<int>(FLAGS_len,sizeof(ValType)), rc->local_mr.value().lkey);
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

auto core_eval_v(const XKey &key, const Arc<RC> &rc, char *my_buf, R2_ASYNC)
    -> ValType {
  // evaluate a get() req
  const usize read_sz = DBTreeV::Leaf::inplace_value_end_offset();

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
    DBTreeV::Leaf *node = reinterpret_cast<DBTreeV::Leaf *> (my_buf + read_sz * (p - ns));
    auto idx = node->search(key);
    if (idx) {
      // fetch the value

      auto val_addr = node->get_value_raw(idx.value()); // get the pointer
      ASSERT(val_addr.get_sz() == FLAGS_len);

      AsyncOp<1> op;
      op.set_read()
        .set_rdma_rbuf(//(const u64 *)(tts.at(m)[p]),
                       val_addr.get_ptr<u64>(),
                       rc->remote_mr.value().key)
        .set_payload(my_buf, val_addr.get_sz(), rc->local_mr.value().lkey);
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
