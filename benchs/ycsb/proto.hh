#pragma once

#include "../../deps/r2/src/common.hh"

namespace xstore {

using namespace r2;

enum RPCId {
  META = 0,
};

// metadata description of the xcache meta
struct __attribute__((packed)) ReplyMeta {
  // how many sub-models in the second layer?
  u32 num_sub;
  // the dispatcher size
  u32 dispatcher_sz;
  // the buffer to store the model data
  u64 model_buf;
  // the buffer to store the TT data
  u64 tt_buf;
};
} // namespace xstore
