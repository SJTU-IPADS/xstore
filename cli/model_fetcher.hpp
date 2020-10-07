#pragma once

#include "marshal.hpp"
#include "../server/internal/table.hpp"

namespace fstore {

using namespace r2;
using namespace r2::rpc;

using namespace server;

using namespace rdmaio;

class ModelFetcher {
 public:
  static SC *bootstrap_remote_sc(u64 tableid,
                                 RPC &rpc,const Addr &server_addr,
                                 RCQP *qp,RScheduler &coro,handler_t &h) {
    // fetch the meta data
    auto table_config = fetch_meta(tableid,server_addr,rpc,coro,h);

    // fetch the learned index structure
    auto first_stage  = fetch_one_stage(table_config.first_buf_addr,table_config.first_buf_sz,qp,coro,h);
    auto second_stage  = fetch_one_stage(table_config.second_buf_addr,table_config.second_buf_sz,qp,coro,h);

    // fetch the additional mapping table
    auto mega = fetch_mega(table_config.mega_addr, table_config.mega_sz, qp, coro, h);
    return new SC(first_stage,second_stage,table_config.total_keys,table_config.key_n,mega);
  }

  /*!
    Fetch the model data using RPC
  */
  static TableModelConfig fetch_meta(u64 table_id,const Addr &server_addr,
                                     RPC &rpc,RScheduler &coro,handler_t &h) {

    char reply_buf[1024];
    memset(reply_buf,0,1024);
    static_assert(1024 > sizeof(TableModelConfig),"");

    TableModelConfig res = {};

    auto &factory = rpc.get_buf_factory();
    auto send_buf = factory.alloc(128);

    Marshal<TableModel>::serialize_to({.id = table_id },send_buf);

    auto ret = rpc.call({.cor_id = coro.cur_id(),.dest = server_addr }, MODEL_META,
                        { .send_buf  = send_buf,.len = sizeof(TableModel),
                          .reply_buf = reply_buf,.reply_cnt = 1});
    //coro.wait_for(50000000L);

    coro.pause_and_yield(h);
    factory.dealloc(send_buf);
    //LOG(4) << "fetch meta sz: " << res.mega_sz;
    return *((TableModelConfig *)reply_buf);
  }

  static std::string fetch_mega(u64 mega_addr,u64 sz,
                                RCQP *qp,RScheduler &coro,handler_t &h) {

    char *local_buf = (char *)(AllocatorMaster<0>::get_thread_allocator()->alloc(sz));
    ASSERT(local_buf != nullptr);

    RdmaFuture::send_wrapper(coro,qp,coro.cur_id(),
                             {.op = IBV_WR_RDMA_READ,
                              .flags = IBV_SEND_SIGNALED,
                              .len   = static_cast<u32>(sz),
                              .wr_id = coro.cur_id()
                             },
                             {.local_buf = local_buf,
                              .remote_addr = mega_addr,
                              .imm_data = 0});
    coro.wait_for(5000000000L);
    coro.pause_and_yield(h);

    //LOG(4) << "fetch mega siz: " << sz;
    // parse the stage from the fetched stage
    auto ret = std::string(local_buf,sz);
    AllocatorMaster<0>::get_thread_allocator()->free(local_buf);
    return ret;
  }

  static std::vector<std::string> fetch_one_stage(u64 buf_addr,u64 sz,
                                                  RCQP *qp,RScheduler &coro,handler_t &h) {
    //LOG(4) << "fetch one stage: " << buf_addr << "; with sz: " << sz;
    ASSERT(sz != 0 && buf_addr != 0);
    char *local_buf = (char *)(AllocatorMaster<0>::get_thread_allocator()->alloc(sz));
    ASSERT(local_buf != nullptr);
    RdmaFuture::send_wrapper(coro,qp,coro.cur_id(),
                             {.op = IBV_WR_RDMA_READ,
                              .flags = IBV_SEND_SIGNALED,
                              .len   = static_cast<u32>(sz),
                              .wr_id = coro.cur_id()
                             },
                             {.local_buf = local_buf,
                              .remote_addr = buf_addr,
                              .imm_data = 0});
    coro.wait_for(5000000000L);
    coro.pause(h);

    // parse the stage from the fetched stage
    auto ret = ModelDescGenerator::deserialize_one_stage(local_buf);
    AllocatorMaster<0>::get_thread_allocator()->free(local_buf);
    return ret;
  }

};

} // fstore
