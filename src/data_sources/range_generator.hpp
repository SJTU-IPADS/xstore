#pragma once

#include "../common.hpp"
#include "datastream/stream.hpp"

namespace fstore {

namespace sources {

class RangeGenerator : public datastream::StreamIterator<u64,u64> {
 public:
  /*!
    Generate u64 from [start, end], inclusively
  */
  RangeGenerator(u64 start_w_id,u64 end_w_id,u64 seed = 0xdeadbeaf) :
      start(start_w_id),
      end(end_w_id),
      rand(seed) {
    begin();
  }

  void begin() override {
    current = start;
  }

  bool valid() override {
    return current <= end;
  }

  void next() override {
    current += 1;
  }

  u64 key() override {
    return current;
  }

  u64 value() override {
    return rand.next();
  }
 private:
  const u64 start;
  const u64 end;
  u64       current;

  r2::util::FastRandom rand;
};

}

} // end namespace fstore
