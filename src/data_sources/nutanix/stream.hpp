#pragma once

#include "../../common.hpp"
#include "../../datastream/stream.hpp"

namespace fstore {

namespace sources {

// The naive generator will generate keys as 0,1,2,...
class NutNaive : public datastream::StreamIterator<u64, u64>
{
  const u64 total_keys;
  u64 current_loaded = 0;
  r2::util::FastRandom rand;

public:
  NutNaive(u64 total_keys, u64 seed = 0xdeadbeaf) : total_keys(total_keys),
                                                        rand(seed)
  {
  }

  void begin() override {
    current_loaded = 0;
  }

  bool valid() override {
    return current_loaded < total_keys;
  }

  void next() override {
    current_loaded += 1;
  }

  u64 key() override {
    return current_loaded;
  }

  u64 value() override {
    return key(); // same as value
  }

};
}
}
