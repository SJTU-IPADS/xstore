#pragma once

#include "../deps/r2/src/common.hh"

namespace xstore {

namespace bench {

using namespace r2;

/*!
 * Statictics used for multi-thread reporting.
 * The structure is 128-byte padded and aligned to avoid false sharing.
 */
struct alignas(128) Statics
{

  typedef struct
  {
    u64 counter = 0;
    u64 counter1 = 0;
    u64 counter2 = 0;
    u64 counter3 = 0;
    double lat = 0;

    double lat1 = 0;
  } data_t;
  data_t data;

  char pad[128 - sizeof(data)];

  // methods
  void increment() { data.counter += 1; }

  void increment_gap_1(u64 d) { data.counter1 += d; }

  void set_lat(const double& l) { data.lat = l; }
};

} // namespace bench

} // namespace xstore
