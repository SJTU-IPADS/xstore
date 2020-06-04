#pragma once

#include "./alloc_trait.hh"

namespace xstore {

namespace xkv {

// a slab allocator
// S indicates the allocate memory block sz
template <usize S> class XAlloc : public AllocTrait<XAlloc, S> {
  char *mem_pool = nullptr;
  const usize total_sz = 0;
  usize cur_alloc_sz = 0;

public:
  XAlloc(char *m, const usize &t) : mem_pool(m), total_sz(t) {}

  // interfaces
  auto alloc_impl() -> Option<char *> {
    if (cur_alloc_sz + S < total_sz) {
      // has free space
      auto res = mem_pool + cur_alloc_sz;
      cur_alloc_sz += S;
      return res;
    }
    return {};
  }

  auto dealloc_impl(char *data) { ASSERT(false) << "not implemented"; }
};

} // namespace xkv

} // namespace xstore
