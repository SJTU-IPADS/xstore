#pragma once

#include "common.hpp"

namespace fstore {

namespace bench {

using namespace fstore;

/*!
 * Statictics used for multi-thread reporting.
 * The structure is 128-byte padded and aligned to avoid false sharing.
 */
struct alignas(128) Statics {
  typedef struct {
    u64    counter = 0;
    u64    counter1 = 0;
    u64    counter2 = 0;
    u64    counter3 = 0;
    u64    padding  = 0;
    double lat     = 0;
  } data_t;

  data_t data;

  void set_lat(double l) {
    data.lat = l;
  }

  void increment() {
    data.counter += 1;
  }

  void increment1() {
    data.counter1 += 1;
  }

  void increment2() {
    data.counter2 += 1;
  }

  void increment3() {
    data.counter3 += 1;
  }

 private:
  static_assert(sizeof(data) < 128, "The data should be less than 128");
  char pad[128 - sizeof(data)];
};

}

} // namespace fstore
