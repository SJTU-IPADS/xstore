#pragma once

#include "common.hpp"
#include <string.h>

namespace fstore {

template <typename T>
class Marshal {
 public:
  /*!
    Simple helper function which write value to buffers
   */
  static inline char *serialize_to(const T &t,char *addr) {
    memcpy(addr,&t,sizeof(T));
    return addr + sizeof(T);
  }

  static inline T deserialize(const char *buf,u64 size) {
    if(size < sizeof(T))
      ASSERT(false);
    T res;
    memcpy(&res,buf,sizeof(T));
    return res;
  }

  static inline T extract(const char *buf) {
    const T *ptr = reinterpret_cast<const T *>(buf);
    return *ptr;
  }

  static inline T extract_with_inc(char *&buf) {
    const T* ptr = reinterpret_cast<const T*>(buf);
    buf += sizeof(T);
    return *ptr;
  }

  static inline Option<T> deserialize_opt(const char *buf,u64 size) {
    if(size < sizeof(T))
      return {};
    T res;
    memcpy(&res,buf,sizeof(T));
    return res;
  }
};

} // fstore
