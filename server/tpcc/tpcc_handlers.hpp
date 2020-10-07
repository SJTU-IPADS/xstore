#pragma once

#include "r2/src/rpc/rpc.hpp"

#include "marshal.hpp"

#include "./tpcc_proto.hpp"
#include "./bootstrap.hpp"

namespace fstore {

namespace server {

extern Tables global_table;

extern thread_local RCQP *self_qp;

class TPCCHandlers
{
public:
  static void fo(RPC& rpc, const Req::Meta& ctx, const char* msg, u32 payload)
  {
    auto& factory = rpc.get_buf_factory();
    char* reply_buf = factory.get_inline();

    FOArg *arg = (FOArg *)msg;
    auto leaf = global_table.get_table(0).data.safe_find_leaf_page(arg->seek_key);
    if (leaf != nullptr) {
      *((u8 *)reply_buf) = leaf->num_keys;
    }
    rpc.reply_async(ctx, reply_buf, sizeof(u8));
  }

  // simulate the process of fetch orderlines
  static void fols(RPC& rpc, const Req::Meta& ctx, const char* msg, u32 payload)
  {
    auto& factory = rpc.get_buf_factory();
    char* reply_buf = factory.get_inline();

    FOArg* arg = (FOArg*)msg;
    auto leaf = orderline_table.safe_find_leaf_page(arg->seek_key);
    u64 count = 0;

    while (count < 15 && leaf != nullptr) {
      leaf->sanity_check();
      for (uint i = 0; i < leaf->num_keys; ++i) {
        u64 v = (u64)(leaf->values[i]);
        reply_buf[count] = (u8)(v);
        count += 1;
      }
      leaf = leaf->right;
    }
    rpc.reply_async(ctx, reply_buf, sizeof(u8) * 15);
  }

  // execute a simplified TPC-C new TX using optimistic concurrency control
  static void no(RPC& rpc, const Req::Meta& ctx, const char* msg, u32 payload)
  {
    NOArg *arg = (NOArg *)msg;

    // now execute the TX body
  retry : {
      // execute phase
      auto it_d = district_table.find(district_key(arg->warehouse_id,arg->district_id).v);
      ASSERT(it_d != district_table.end()) << "get district id: " << arg->district_id;
      district_value &d_v = it_d->second;
      const u64 dversion = d_v.lock;

      const u64 next_id = d_v.next_o_id;

      std::vector<u64> svs;
      std::vector<stock_value *> stocks;

      for (uint i = 0; i < arg->num_stocks; ++i) {
        auto it_s = stock_table.find(arg->stocks[i]);
        ASSERT(it_s != stock_table.end()) << "stock key: " << arg->stocks[i];
        stock_value &sv = it_s->second;
        stocks.push_back(&sv);
        svs.push_back(sv.lock);
      }

      // commit phase

      // c1. lock all rwset
      if (unlikely(!__sync_bool_compare_and_swap(&(d_v.lock), dversion, 0))) {
        // lock failed
        goto retry;
      }

      for (uint i = 0; i < arg->num_stocks; ++i) {
        auto sp = stocks[i];
        if(unlikely(!__sync_bool_compare_and_swap(&(sp->lock),svs[i],0)))
          goto retry;
      }


      // c2 + 3. write back + release lock
      // first insert the order
      {
        d_v.next_o_id += 1;
        d_v.lock = dversion + 1;

        order_key k(arg->warehouse_id, arg->district_id, next_id);
        oidx_key sk(arg->warehouse_id, arg->district_id, arg->cust_id, next_id);
        no_key no_k(arg->warehouse_id,arg->district_id,next_id);

        // first insert the order
#if 1
        auto ptr = order_table.safe_get_with_insert(k.v);
        no_table.safe_get_with_insert(no_k.v);
        ASSERT(ptr != nullptr);
        order_value* ov =
          (order_value*)(AllocatorMaster<>::get_thread_allocator()->alloc(
            sizeof(order_value)));
        ASSERT(ov != nullptr);
        ov->cid = arg->cust_id;
        ov->ol_cnt = arg->num_stocks;
        *ptr = ov;
#endif
        r2::compile_fence();

        oidx_key idxk(
          arg->warehouse_id, arg->district_id, arg->cust_id, next_id);
        auto ptridx =
          global_table.get_table(0).data.safe_get_with_insert(idxk.v);
        ASSERT(ptridx != nullptr);
      }

      for (uint i = 0; i < arg->num_stocks; ++i) {
        auto sp = stocks[i];
        sp->s_quantity += 1;
        r2::compile_fence();
        sp->lock = svs[i] += 1;

#if 1
        r2::compile_fence();
        ol_key k(arg->warehouse_id,arg->district_id,next_id,i);
        auto ptr = orderline_table.safe_get_with_insert(k.v);
        *ptr = new ol_value;
#endif
      }
    }

#if 1
    // emulate two RDMA writes
    static thread_local char* local_log_buf =
      (char*)(AllocatorMaster<>::get_thread_allocator()->alloc(4096));
    self_qp->send({ .op = IBV_WR_RDMA_WRITE,
                    .flags = IBV_SEND_SIGNALED,
                    .len = 1024,
                    .wr_id = 0 },
                  { .local_buf = local_log_buf,
                    .remote_addr = arg->warehouse_id * 4096,
                    .imm_data = 0 });
    ibv_wc wc;
    QPUtily::wait_completion(self_qp->cq_, wc);
#endif

    auto& factory = rpc.get_buf_factory();
    char* reply_buf = factory.get_inline();
    rpc.reply_async(ctx, reply_buf,0);
  }

  static void register_all(RPC& rpc)
  {
    rpc.register_callback(NO_ID, no);
    rpc.register_callback(FETCH_ORDER, fo);
    rpc.register_callback(FETCH_OLS,fols);
  }
};

} // namespace server
}
