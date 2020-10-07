#pragma once

#include "r2/src/allocator.hpp"
#include "r2/src/futures/rdma_future.hpp"

namespace fstore {

extern __thread u64 data_transfered;

extern Tree sample_cache;

#define COUNT_LAT 1

class NTree
{
public:
#if COUNT_LAT
  u64 rdma_index;
  u64 rdma_value;
  u64 total;
#endif
  NTree(RCQP *qp,u64 page_start,u64 page_area_sz,uint depth,u64 seed, bool verbose=false) :
      page_addr(page_start), page_area_sz(page_area_sz),qp(qp), max_depth(depth),rand(seed) {
    //assert(max_depth > 1);
    assert(qp != nullptr);

    //max_depth += 1;

    if(verbose) {
      LOG(3) << "network tree use search depth: " << max_depth << "; each with Inner sz: " << sizeof(Inner)
             << "; cache depth: " << sample_cache.depth;
    }
    ASSERT(sample_cache.depth >= 0);
  }

  inline Option<u64> emulate_roundtrip(char *local_buf,const usize &num,
                                       RScheduler &coro,handler_t &h) {
    //auto depth = 4;
    for (uint i = 0; i < num; ++i) {
      auto addr = page_addr + rand.next() % page_area_sz;
      qp->send({ .op = IBV_WR_RDMA_READ,
                 .flags = IBV_SEND_SIGNALED,
                 .len = sizeof(Inner),
                 .wr_id = coro.cur_id() },
               { .local_buf = local_buf, .remote_addr = addr, .imm_data = 0 });
      RdmaFuture::spawn_future(coro, qp, 1);
      coro.pause(h);
    }


    return 0;
  }

  /**
   * emulated get, using the maximum depth of the tree
   */
  inline Option<u64> get_addr(const u64 &key,char *local_buf,
                              RScheduler &coro,handler_t &h) {
    u64 res = 0;
    //res += sample_cache.traverse_to_depth(key,1000);

    u64 addr = 0;

    auto depth = max_depth;
    //depth = 4;
    depth = 0;
    for(uint i = 0;i < depth;++i) {
    //for(uint i = 0;i < 4;++i) {
      addr = page_addr + rand.next() % page_area_sz;
      qp->send({.op = IBV_WR_RDMA_READ,
                .flags = IBV_SEND_SIGNALED,
                .len   = sizeof(Inner),
                //.len = 64,
                .wr_id = coro.cur_id()},
        {.local_buf = local_buf,
         .remote_addr = addr,
         .imm_data = 0});
      RdmaFuture::spawn_future(coro,qp,1);
      coro.pause(h);
    }
    data_transfered += (sizeof(Inner) * depth);

    total = 0;
    rdma_index = 0;
    rdma_value = 0;
#if COUNT_LAT
    u64 is = read_tsc();
#endif
    //addr = page_addr + rand.next() % page_area_sz;
    //addr = 0;
    addr = 73;
    //addr = key & page_area_sz + page_addr;
    // then read the leaf
    qp->send(
      {
        .op = IBV_WR_RDMA_READ, .flags = IBV_SEND_SIGNALED,
        //.len = Leaf::value_offset(0),
        .len = sizeof(u64),
        .wr_id = coro.cur_id()
      },
      { .local_buf = local_buf, .remote_addr = addr, .imm_data = 0 });
    RdmaFuture::spawn_future(coro, qp, 1);
    coro.pause(h);
#if COUNT_LAT
    r2::compile_fence();
    rdma_index = read_tsc() - is;
#endif

    data_transfered += Leaf::value_offset(0);

#if COUNT_LAT
    u64 iv = read_tsc();
#endif
#if 0
    addr = addr + Leaf::value_offset(4 % 15);
    // use one last roundtrip to fetch the value
    qp->send({.op = IBV_WR_RDMA_READ,
              .flags = IBV_SEND_SIGNALED,
              .len   = ValType::get_payload(),
              .wr_id = coro.cur_id()},
      {.local_buf = local_buf,
       .remote_addr = addr,
       .imm_data = 0});
    RdmaFuture::spawn_future(coro,qp,1);
    coro.pause(h);
    data_transfered += ValType::get_payload();
#endif

#if COUNT_LAT
    r2::compile_fence();
    rdma_value = read_tsc() - iv;
    total = read_tsc() - is;
#endif

    return Option<u64>(*((u64 *)local_buf) + res);
  }

 private:
  RCQP *qp           = nullptr;
  u64   page_addr    = 0;
  u64   page_area_sz = 0;
  uint  max_depth = 0;
  r2::util::FastRandom rand;
};
}
