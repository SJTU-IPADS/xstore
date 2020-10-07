#pragma once

#include "common.hpp"

#include <functional>
#include <pthread.h>

namespace r2 {

/*!
  This is a simple wrapper over pthread, which uses a more abstracted way to create threads.
  T: the return type of the thread.

  Example:
  To create one thread using the function `fn run() -> T`, one can use the following way:
    auto t = Thread<T>([]() -> T {
        T res;
        return res;
    });
    t.start(); // really run the thread
    t.join();  // wait for it to stop
 */
template<typename T = int>
class alignas(128) Thread
{
  using thread_body_t = std::function<T(void)>;

  thread_body_t core_func;
  T res;
  pthread_t pid; // pthread id

public:
  explicit Thread(const thread_body_t& b)
    : core_func(b)
  {}

  void start()
  {
    pthread_attr_t attr;
    ASSERT(pthread_attr_init(&attr) == 0);
    ASSERT(pthread_create(&pid, &attr, pthread_bootstrap, (void*)this) == 0);
    ASSERT(pthread_attr_destroy(&attr) == 0);
  }

  T join()
  {
    ASSERT(pthread_join(pid, nullptr) == 0);
    return get_res();
  }

  T get_res() const { return res; }

private:
  // TODO: what if the sizeof(T) is very large?
  static_assert(sizeof(T) < (128 - sizeof(thread_body_t) - sizeof(pthread_t)),
                "xx");
  char padding[128 - (sizeof(thread_body_t) + sizeof(T) + sizeof(pthread_t))];

  static void* pthread_bootstrap(void* p)
  {
    Thread* self = static_cast<Thread*>(p);
    self->res = self->core_func();
    return nullptr;
  }
};

}
