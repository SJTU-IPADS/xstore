#pragma once

#include "common.hpp"

#include <pthread.h>
#include <functional>

namespace fstore {

template <typename T = int>
class alignas(128) Thread {
  typedef std::function<T (void)> thread_body_t;
 public:
  explicit Thread(const thread_body_t &b) : core_func(b) {
  }

  void start() {
    pthread_attr_t attr;
    ASSERT(pthread_attr_init(&attr) == 0);
    ASSERT(pthread_create(&pid, &attr, pthread_bootstrap, (void *) this) == 0);
    ASSERT(pthread_attr_destroy(&attr) == 0);
  }

  T join() {
    ASSERT(pthread_join(pid,nullptr) == 0);
    return get_res();
  }

  T get_res() const {
    return res;
  }

 private:
  thread_body_t core_func;
  T res;
  pthread_t  pid;      // pthread id

  // TODO: what if the sizeof(T) is very large?
  static_assert(sizeof(T) < (128 - sizeof(thread_body_t) - sizeof(pthread_t)),"xx");
  char       padding[128 - (sizeof(thread_body_t) + sizeof(T) + sizeof(pthread_t))];

  static void *pthread_bootstrap(void *p) {
    Thread *self = static_cast<Thread *>(p);
    self->res = self->core_func();
    return nullptr;
  }
};

}
