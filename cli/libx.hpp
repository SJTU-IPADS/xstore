#pragma once

#include "./fclient.hpp"
#include "r2/src/rpc/rpc.hpp"

namespace fstore {

class XClient
{
  FClient* xcache = nullptr;
  r2::rpc::RPC* rpc = nullptr;
  const Addr addr;

public:
  XClient(const Addr& addr, FClient* x, r2::rpc::RPC* r)
    : xcache(x)
    , rpc(r)
    , addr(addr)
  {}

  IOStatus put(const u64 &key, const ValType &val, char *send_buf, R2_ASYNC) {
    char *msg_buf = send_buf + 64;
    char reply_buf[64];
    GetPayload req = { .table_id = 0, .key = key };
    Marshal<GetPayload>::serialize_to(req, msg_buf);
    memcpy(msg_buf + sizeof(GetPayload), &val,sizeof(ValType));

    auto ret = rpc->call({ .cor_id = R2_COR_ID(), .dest = addr },
                        PUT_ID,
                        { .send_buf = msg_buf,
                          .len = sizeof(GetPayload) + sizeof(ValType),
                          .reply_buf = reply_buf,
                          .reply_cnt = 1 });
    auto res = R2_PAUSE;
    return res;
  }

  FClient::SeekResult scan(const u64& key,
                           const usize num,
                           char* send_buf,
                           R2_ASYNC)
  {
    auto res = xcache->seek(key, send_buf, R2_ASYNC_WAIT);
    switch (std::get<0>(res)) {
      case Ok: {
        // we fetch all the page back
        u64 estimated_pages = num / IM * 2;
        auto page_num = std::get<0>(std::get<1>(res));

        xcache->scan_with_seek(std::get<1>(res), num, send_buf, R2_ASYNC_WAIT);
      }
      default:
        rpc_scan(key, num, send_buf,R2_ASYNC_WAIT);
        break;
    }
    return res;
  }

  FClient::SeekResult rpc_scan(const u64& key,
                           const usize num,
                           char* send_buf,
                               R2_ASYNC) {
    char* msg_buf = send_buf + 64;
    // fallback to RPC for execution
    ScanPayload req = { .table_id = 0,
                        .start = key,
                        .num = num };
    Marshal<ScanPayload>::serialize_to(req, msg_buf);
    ASSERT(req.num > 0);

    int expected_replies =
      std::ceil((num * ValType::get_payload()) / 4000.0);

    auto ret = rpc->call({ .cor_id = R2_COR_ID(), .dest = addr },
                        SCAN_RPC,
                        { .send_buf = msg_buf,
                          .len = sizeof(ScanPayload),
                          .reply_buf = send_buf,
                          .reply_cnt = expected_replies });
    ASSERT(ret == SUCC);
    R2_PAUSE;
    return std::make_pair(Ok, std::make_pair(0, 0));
  }

  FClient::GetResult get_rpc(const u64& key, char* send_buf, R2_ASYNC)
  {
    GetPayload req = { .table_id = 0, .key = key };
    char* msg_buf = send_buf + 64; // add a padding to store rpc header
    Marshal<GetPayload>::serialize_to(req, msg_buf);

    auto ret = rpc->call({ .cor_id = R2_COR_ID(), .dest = addr },
                         GET_ID,
                         { .send_buf = msg_buf,
                           .len = sizeof(GetPayload),
                           .reply_buf = send_buf,
                           .reply_cnt = 1 });

    auto res = R2_PAUSE;
    ASSERT(res == SUCC);
    return std::make_pair(Ok, 0);
  }

  // a wrapper to the FClient, that fallback to RPC under xcache invalidation
  FClient::GetResult get(const u64& key, char* send_buf, R2_ASYNC)
  {
    auto predict = xcache->get_predict(key);
    auto res = xcache->get_addr(key, predict, send_buf, R2_ASYNC_WAIT2);

    switch (std::get<0>(res)) {
      case SearchCode::Ok:
        break;
      case SearchCode::None:
        break;
      case SearchCode::Fallback:
      case SearchCode::Invalid:
      case SearchCode::Unsafe:
        // fallback to RPC for execution
        {
          GetPayload req = { .table_id = 0, .key = key };
          char* msg_buf = send_buf + 64; // add a padding to store rpc header
          Marshal<GetPayload>::serialize_to(req, msg_buf);

          auto ret = rpc->call({ .cor_id = R2_COR_ID(), .dest = addr },
                               GET_ID,
                               { .send_buf = msg_buf,
                                 .len = sizeof(GetPayload),
                                 .reply_buf = send_buf,
                                 .reply_cnt = 1 });

          auto res = R2_PAUSE;
          ASSERT(res == SUCC);
        }
        break;
      default:
        ASSERT(false) << "unreachable!";
    }
    return res;
  }
};

}
