#pragma once

#include "../deps/r2/src/common.hh"

namespace xstore {

namespace util {

using namespace r2;

template <typename T> class AvgReport {
public:
  double average = 0;
  T min;
  T max;
  u64 num = 1;

  AvgReport()
      : min(std::numeric_limits<T>::max()), max(std::numeric_limits<T>::min()) {
  }

  void add(const T &v) {
    min = std::min(v, min);
    max = std::max(v, max);
    average += (static_cast<double>(v) - average) / static_cast<double>(num);
    num += 1;
  }

  void clear() {
    average = 0;
    min = std::numeric_limits<T>::max();
    max = std::numeric_limits<T>::min();
    num = 1;
  }
};
} // namespace util

} // namespace xstore
