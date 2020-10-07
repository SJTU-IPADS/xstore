#pragma once

#include "common.hpp"
#include "port/atomic.hpp"

namespace fstore {

namespace utils {

/*!
  The version of spin lock without padding
 */
class SpinLockWOP {
public:
  volatile u16 lock_ = 0;
  SpinLockWOP() = default;

  inline void lock() {
    while (1) {
      if (!xchg16((u16 *)&lock_, 1)) return;

      while(lock_) cpu_relax();
    }
  }

  inline void unlock() {
    barrier();
    lock_ = 0;
  }

  inline u16 try_lock() {
  	return xchg16((u16 *)&lock_, 1);
  }

  inline u16 is_locked() const {
    return lock_;
  }
};

class SpinLock  {
 public:
  //0: free, 1: busy
  //occupy an exclusive cache line
  volatile u8 padding1[32];
  volatile u16 lock_ = 0;
  volatile u8 padding2[32];
 public:

  SpinLock() = default;

  inline void lock() {
    while (1) {
      if (!xchg16((u16 *)&lock_, 1)) return;

      while(lock_) cpu_relax();
    }
  }

  inline void unlock() {
    barrier();
    lock_ = 0;
  }

  inline u16 try_lock() {
  	return xchg16((u16 *)&lock_, 1);
  }

  inline u16 is_locked() const {
    return lock_;
  }
};

} // utils

} // fstore
