#pragma once

#include "scan_context.hpp"

namespace fstore {

using namespace r2;
using namespace r2::rpc;

using namespace rdmaio;

const i32 max_keys_scan = 128;

class RPCScan
{
public:
  RPCScan(int tableid,
          const std::tuple<u64, u64>& range,
          i32 keys_per_batch,
          // some other RDMA data structures
          RPC& rpc,
          const Addr& server_addr,
          RScheduler& coro,
          handler_t& h)
    : keys_per_batch(std::min(max_keys_scan, keys_per_batch))
    , context(INVALID_CURSOR, 0, range)
  {

    auto& factory = rpc.get_buf_factory();
    auto send_buf = factory.get_inline();

    Marshal<ScanPayload>::serialize_to({ .table_id = tableid,
                                         .start = std::get<0>(range),
                                         .num = std::get<1>(range),
                                         .key_per_req = keys_per_batch },
                                       send_buf);

    // we make an RPC call to fetch the remote keys
    auto ret = rpc.call({ .cor_id = coro.cur_id(), .dest = server_addr },
                        SCAN_RPC,
                        { .send_buf = send_buf,
                          .len = sizeof(ScanPayload),
                          .reply_buf = (char*)(&cached_keys),
                          .reply_cnt = 1 });

    coro.pause_and_yield(h);
    ASSERT(cached_keys.num <= max_keys_scan);
  }

private:
  ScanContext context;
  u64 keys_per_batch;
  struct
  {
    u32 num;
    u64 keys[max_keys_scan];
  } cached_keys;
};

} // fstore
