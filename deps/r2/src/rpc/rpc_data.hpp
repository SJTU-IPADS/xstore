#pragma once

#include "../msg/protocol.hpp"

namespace r2 {

namespace rpc {

enum MsgType {
  REQ = 1,  // RPC request
  REPLY     // RPC reply
};

/**
 * RPC request structure
 */
class Req {
 public:
  struct Meta {
    u8      cor_id;
    Addr    dest;
  };

  struct Arg {
    const char *send_buf  = nullptr;
    u32         len       = 0;
    char       *reply_buf = nullptr;
    u32         reply_cnt = 0;
  };

  // internal data structures used in RPC
  struct __attribute__ ((packed)) Header {
    u32 type : 2;
    u32 rpc_id :  5;
    u32 payload : 18;
    u32 cor_id     : 7;
  }  __attribute__ ((aligned (sizeof(u64))));

 public:
  static int sizeof_header(void) {
    return sizeof(Header);
  }
};  // end class RPC data

/**
 * record the pending reply structures
 */
struct Reply {
  char *reply_buf   = nullptr;
  int   reply_count = 0;
};

} // end namespace rpc

} // end namespace r2
