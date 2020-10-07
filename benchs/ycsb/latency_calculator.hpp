#pragma once

#include "common.hpp"

namespace fstore {

namespace bench {

/*!
  Only record the *average* latency.
 */
class FlatLatRecorder {
 public:
  FlatLatRecorder() = default;

  double get_lat() const { return cur_lat; }

  void add_one(double lat) {
    cur_lat += (lat - cur_lat) / counts;
    counts += 1;
  }

 private:
  double cur_lat = 0.0;
  u64    counts  = 1;
};

/*!
  Record all latencies, including 50%, 90% and 99%.
*/
class DetailedLatRecorder {

};

} // bench

} // fstore
