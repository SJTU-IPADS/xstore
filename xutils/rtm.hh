#pragma once

#include <immintrin.h>

#include "./spin_lock.hh"

namespace xstore {

namespace util {

// constants for tuning RTM performance
const usize max_abort_capacity = 1;
const usize max_abort_nested = 0;
const usize max_abort_zero = 0;
const usize max_abort_conflict = 10;

// magic numbers
const usize magic_explict_abort_flag = 0x73;

struct RTMScope {
  SpinLock *fallback_lock = nullptr;

  // used to record RTM abort data
  int retry = 0;
  int conflict = 0;
  int capacity = 0;
  int nested = 0;
  int zero = 0;

  explicit RTMScope(SpinLock *l) : fallback_lock(l) {

    while (true) {
      unsigned stat;
      stat = _xbegin();
      if (stat == _XBEGIN_STARTED) {
        // Put the global lock into read set
        if (fallback_lock && fallback_lock->is_locked()) {
          _xabort(0xff);
        }
        return;

      } else {
        // retry
        retry++;
        if ((stat & _XABORT_NESTED) != 0)
          nested++;
        else if (stat == 0)
          zero++;
        else if ((stat & _XABORT_CONFLICT) != 0) {
          conflict++;
        } else if ((stat & _XABORT_CAPACITY) != 0)
          capacity++;

        if ((stat & _XABORT_EXPLICIT) &&
            _XABORT_CODE(stat) == magic_explict_abort_flag) {
          while (fallback_lock && fallback_lock->is_locked()) {
            _mm_pause();
          }
        }

        int step = 1;

        if (nested > max_abort_nested)
          break;
        if (zero > max_abort_zero / step) {
          break;
        }

        if (capacity > max_abort_capacity / step) {
          break;
        }
        if (conflict > max_abort_conflict / step) {
          break;
        }
      }
    }
    fallback_lock->lock();
  }

  inline ~RTMScope(){
    if (_xtest()) {
      _xend();
      return;
    }

    if (fallback_lock && fallback_lock->is_locked()) {
      fallback_lock->unlock();
    }
  }
};

} // namespace util

} // namespace xstore
