#pragma once

#include "./alloc_trait.hh"

namespace xstore {

namespace xkv {

// a slab allocator
// S indicates the allocate memory block sz
template <usize S> class XAlloc : public AllocTrait<XAlloc> {
  char *mem_pool = nullptr;
  const usize total_sz = 0;

public:
  // interfaces
  auto alloc_impl() -> Option<char *> { return {}; }

  auto dealloc_impl(char *data) { ASSERT(false) << "not implemented"; }
};
} // namespace xkv

} // namespace xstore
