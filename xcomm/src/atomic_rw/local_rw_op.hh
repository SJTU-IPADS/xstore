#pragma once

#include <string.h>

#include "./rw_trait.hh"
#include "./ptr.hh"

namespace xstore {

namespace xcomm {

namespace rw {

class LocalRWOp : public ReadWriteTrait<LocalRWOp> {
public:
  LocalRWOp() = default;

  auto read_impl(const MemBlock &src, const MemBlock &dest) -> Result<> {
    if (src.sz > dest.sz) {
      // size not match
      return ::rdmaio::Err();
    }
    // memcpy version
    memcpy(dest.mem_ptr, src.mem_ptr, dest.sz);
    return ::rdmaio::Ok();
  }
};

class OrderedRWOp : public ReadWriteTrait<OrderedRWOp> {
public:
  OrderedRWOp() = default;

  auto read_impl(const MemBlock &src, const MemBlock &dest) -> Result<> {
    usize cur_copy_sz = 0;
    char *sp = reinterpret_cast<char *>(src.mem_ptr);
    char *dp = reinterpret_cast<char *>(dest.mem_ptr);

    while (cur_copy_sz < dest.sz) {
      if (dest.sz - cur_copy_sz < sizeof(u64)) {
        ASSERT(false);
        memcpy(dp, sp, dest.sz - cur_copy_sz);
        break;
      }
      u64 v = read_volatile<u64>(sp);
      write_volatile(dp, v);
      r2::mfence();

      cur_copy_sz += sizeof(u64);
      sp += sizeof(u64);
      dp += sizeof(u64);
    }
    return ::rdmaio::Ok();
  }
};
}

} // namespace xcomm

} // namespace xstore
