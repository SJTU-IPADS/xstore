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
  const u64 total_sz = 0;
  u64 cur_alloc_sz = 0;

public:
  usize cur_alloc_num = 0;

  XAlloc(char *m, const u64 &t) : mem_pool(m), total_sz(t) {}

  // interfaces
  auto alloc_impl() -> ::r2::Option<char *> {
    lock.lock();
    if (cur_alloc_sz + S <= total_sz) {
      // has free space
      auto res = mem_pool + cur_alloc_sz;
      cur_alloc_sz += S;
      cur_alloc_num += 1;
      lock.unlock();
      return res;
    }
    lock.unlock();
    LOG(4) << cur_alloc_sz << " " << total_sz;
    return {};
  }

  auto dealloc_impl(char *data) { ASSERT(false) << "not implemented"; }
};

} // namespace xkv

} // namespace xstore
