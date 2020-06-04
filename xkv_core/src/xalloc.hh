#pragma once

#include "./alloc_trait.hh"

#include "xtree/spin_lock.hh"

namespace xstore {

namespace xkv {

// a slab allocator
// S indicates the allocate memory block sz
template <usize S>
class XAlloc : public AllocTrait<XAlloc<S>, S> {
  ::xstore::xkv::xtree::CompactSpinLock lock;
  char *mem_pool = nullptr;
  const usize total_sz = 0;
  usize cur_alloc_sz = 0;

public:
  XAlloc(char *m, const usize &t) : mem_pool(m), total_sz(t) {}

  // interfaces
  auto alloc_impl() -> Option<char *> {
    lock.lock();
    if (cur_alloc_sz + S <= total_sz) {
      // has free space
      auto res = mem_pool + cur_alloc_sz;
      cur_alloc_sz += S;
      lock.unlock();
      return res;
    }
    lock.unlock();
    return {};
  }

  auto dealloc_impl(char *data) { ASSERT(false) << "not implemented"; }
};

} // namespace xkv

} // namespace xstore
