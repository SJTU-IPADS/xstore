#pragma once

#include <chrono>

namespace r2 {
/**
 * a simple wrapper over std::time API
 */
class Timer {
 public:
  static const constexpr double no_timeout = std::numeric_limits<double>::max();

  Timer(std::chrono::time_point<std::chrono::steady_clock> t = std::chrono::steady_clock::now())
      : start_time_(t) {
  }

  ~Timer() = default;

  template <typename T>
  bool timeout(double count) const {
    return passed<T>() >= count;
  }

  double passed_sec() const {
    return passed<std::chrono::seconds>();
  }

  double passed_msec() const {
    return passed<std::chrono::microseconds>();
  }

  template <typename T> double passed() const {
    return passed<T>(std::chrono::steady_clock::now());
  }

  template <typename T> double
  passed(std::chrono::time_point<std::chrono::steady_clock> tt) const {
    const auto elapsed = std::chrono::duration_cast<T>(
        tt - start_time_);
    return elapsed.count();
  }

  void reset() {
    start_time_ = std::chrono::steady_clock::now();
  }

  Timer& operator=(Timer&) = default;
 private:
  std::chrono::time_point<std::chrono::steady_clock> start_time_;
};

} // end namespace r2
