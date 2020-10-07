#pragma once

#include <pthread.h>

#include "../common.hpp"

namespace fstore {

namespace utils {

class CoreBinder {
 public:
  static void bind(u32 id) {
#ifndef __APPLE__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(2 * id, &cpuset);
    int rc = pthread_setaffinity_np(pthread_self(),
                                    sizeof(cpu_set_t), &cpuset);
    ASSERT(rc == 0) <<  " " << rc << " " << strerror(errno);
#endif
  }
};

}

} // namespace fstore
