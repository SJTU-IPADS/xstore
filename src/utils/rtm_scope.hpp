#pragma once

#include "../common.hpp"
#include "spin_lock.hpp"

namespace fstore {

namespace utils {

template <int MAX_NESTED = 0,int MAX_CAPACITY = 1,int MAX_CONFLICT=100>
class RTMScope {
  SpinLock *fallback_lock;

  // records of runtime aborts
  u8 abort_capacity = 0;
  u8 abort_nested = 0;
  u8 abort_zero = 0;
  u8 abort_conflicts = 0;

 public:
  inline explicit RTMScope(SpinLock *l) : fallback_lock(l) {

    while (true) {
      const u8 fallback_abort_flag = 0xff;
      unsigned stat = _xbegin();

      if(stat == _XBEGIN_STARTED) {

        //Put the global lock into read set
        if(fallback_lock && fallback_lock->is_locked())
          _xabort(fallback_abort_flag);
        return;
      } else {
        if((stat & _XABORT_NESTED) != 0)
          abort_nested += 1;
        else if(stat == 0)
          abort_zero += 1;
        else if((stat & _XABORT_CONFLICT) != 0) {
          abort_conflicts += 1;
        }
        else if((stat & _XABORT_CAPACITY) != 0)
          abort_capacity += 1;
        if ((stat & _XABORT_EXPLICIT) && _XABORT_CODE(stat) == fallback_abort_flag) {
          while(fallback_lock && fallback_lock->is_locked())
            _mm_pause();
        }

        // we check whether retry the RTM execution
        // by comparing all their thresholds
        if (abort_zero ||
            abort_nested > MAX_NESTED ||
            abort_capacity > MAX_CAPACITY ||
            abort_conflicts > MAX_CONFLICT )
          break;
      }
    }
    if (fallback_lock) {
      fallback_lock->lock();
    }
  }

  inline ~RTMScope() {
    if(fallback_lock && fallback_lock->is_locked())
      fallback_lock->unlock();
    else
      _xend();
  }

};

} // namespace utils

} // namespace drtm
