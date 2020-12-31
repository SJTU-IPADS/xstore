#pragma once

#include <cmath>

namespace xstore {

namespace xml {

// common ML utilities
struct Op
{
  template<typename T>
  static inline auto sigmoid(const T& v) -> T
  {
    return 1.0 / (1.0 + std::exp(-v));
  }

  template<typename T>

  static inline auto fast_sigmoid(const T& v) -> T
  {
    return v / (1 + std::abs(v));
  }

  /*!
    Credits: https://stackoverflow.com/questions/10732027/fast-sigmoid-algorithm
   */
  template<typename T>
  static inline auto rough_sigmoid(const T& value) -> T
  {
    auto x = std::abs(value);
    auto x2 = x * x;
    auto e = 1.0f + x + x2 * 0.555f + x2 * x2 * 0.143f;
    return 1.0 / (1.0 + (value > 0 ? 1.0 / e : e));
  }
};
}
}