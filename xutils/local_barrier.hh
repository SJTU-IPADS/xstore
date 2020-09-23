#pragma once

#ifndef __APPLE__ // OSX does not provide pthread barrier
#include <atomic>
#include <pthread.h>
#endif

#include "../deps/r2/src/common.hh"

namespace xstore {

namespace util {

/**
 * a simple wrapper over pthread
 */
class PBarrier {
  pthread_barrier_t barrier_;
  std::atomic<int> wait_num_;

public:
  explicit PBarrier(int num) : wait_num_(num) {
    ASSERT(num > 0);
    pthread_barrier_init(&barrier_, nullptr, num);
  }

  ~PBarrier() { pthread_barrier_destroy(&barrier_); }

  void wait() {
    wait_num_ -= 1;
    pthread_barrier_wait(&barrier_);
  }

  void done() { wait_num_ -= 1; }

  bool ready() const { return wait_num_ == 0; }

  int wait_num() const { return wait_num_; }

  DISABLE_COPY_AND_ASSIGN(PBarrier);
};

} // namespace util

} // namespace xstore

