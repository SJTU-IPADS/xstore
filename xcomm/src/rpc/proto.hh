#pragma once

#include <iostream>

#include "../../../deps/r2/src/common.hh"

/*!
  This file defines common RPC exchange data structures
 */

namespace xstore {

namespace rpc {

using namespace r2;

/*!
  Define the msg type that can be exchanged
 */
enum MsgType {
  Req = 1,      // RPC request
  Reply = 2,    // RPC reply
  Connect = 3,  // msg connect
};

/*!
  Abstract the end-point of Addr
 */
struct __attribute__((packed)) Addr {
  u32 mac_id : 16;
  u32 thread_id : 16;
};

struct __attribute__((packed)) Meta {
  u8 cor_id;
  Addr dest;
};

struct __attribute__((packed)) Header {
  u32 type : 2;
  u32 rpc_id : 5;
  u32 payload : 18;
  u32 cor_id : 7;

  friend std::ostream &operator<<(std::ostream &os, const Header &h) {
    os << "type:" << h.type << "; rpc_id: " << h.rpc_id << "; payload:"  << h.payload
       << " cor_id: " << h.cor_id;
  }
} __attribute__((aligned(sizeof(u64))));

} // namespace rpc

} // namespace xstore
