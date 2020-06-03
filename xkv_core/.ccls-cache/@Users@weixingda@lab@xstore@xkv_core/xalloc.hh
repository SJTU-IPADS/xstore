#pragma once

#include "../deps/r2/src/common.hh"

namespace xstore {

namespace xkv {

using namespace r2;

// a slab allocator
// S indicates the allocate memory block sz
template <usize S> class XAlloc {
  char *mem_pool = nullptr;
  const usize total_sz = 0;

public:
  // interfaces
  auto alloc() -> Option<char *> { return {}; }

  auto dealloc(char *data) { ASSERT(false) << "not implemented"; }
};
} // namespace xkv

} // namespace xstore
