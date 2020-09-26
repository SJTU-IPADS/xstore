#pragma once

#include "../../deps/r2/src/common.hh"

namespace xstore {

using namespace r2;

enum RPCId {
  META = 0,
  GET  = 1,
};

// metadata description of the xcache meta
struct __attribute__((packed)) ReplyMeta {
  // the dispatcher size
  u32 dispatcher_sz;
  u32 total_sz;
  // the buffer to store the model data
  u64 model_buf;
  // the buffer to store the TT data
  u64 tt_buf;
};
} // namespace xstore
