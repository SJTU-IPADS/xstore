#pragma once

#include "r2/src/common.hpp"
#include "rocksdb/slice.h"

namespace fstore {

namespace datastream {

class RocksUtil {
 public:
  /*!
    Convert a given data type to RocksDB's internal data structure.
  */
  template <typename T>
  static inline rocksdb::Slice to_slice(const T &t) {
    return rocksdb::Slice((char *)(&t),sizeof(T));
  }

  template <typename T>
  static inline T from_slice(const rocksdb::Slice &s) {
    T res;
    ASSERT(s.size() >= sizeof(T));
    memcpy(&res,s.data(),sizeof(T));
    return res;
  }
};

}
}
