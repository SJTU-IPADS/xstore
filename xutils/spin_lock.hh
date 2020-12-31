#pragma once

#include "./atomic.hh"

#include "../deps/r2/src/common.hh"

namespace xstore {

namespace util {

using namespace r2;

inline auto cpu_relax() { asm volatile("pause\n" : : : "memory"); }

struct SpinLock {
  // 0: free, 1: busy
  // occupy an exclusive cache line
  volatile u8 padding1[32];
  volatile u16 lock_ = 0;
  volatile u8 padding2[32];

  SpinLock() = default;

  inline void lock() {
    while (1) {
      if (!xchg16((u16 *)&lock_, 1))
        return;

      while (lock_)
        cpu_relax();
    }
  }

  inline void unlock() {
    compile_fence();
    lock_ = 0;
  }

  inline u16 try_lock() { return xchg16((u16 *)&lock_, 1); }

  inline u16 is_locked() const { return lock_; }
};

} // namespace util

} // namespace xstore
