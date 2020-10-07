#pragma once

#include "../common.hpp"

#include <limits>
#include <cmath>

namespace fstore {

namespace utils {

/**
 * Credits:
 * https://github.com/basicthinker/YCSB-C
 */
class ZipFanD {
 public:
  constexpr static const double kZipfianConst = 0.99;
  static constexpr u64 kMaxNumItems = (std::numeric_limits<u64>::max() >> 24);

  ZipFanD(u64 min, u64 max,
          u64 seed,
          double zipfian_const = kZipfianConst) :
      num_items_(max - min + 1), base_(min), theta_(zipfian_const),
      zeta_n_(0), n_for_zeta_(0),
      rand(seed) {
    assert(num_items_ >= 2 && num_items_ < kMaxNumItems);
    zeta_2_ = zeta(2, theta_);
    alpha_ = 1.0 / (1.0 - theta_);

    raise_zeta(num_items_);
    eta_ = eta();

    next();
  }

  ZipFanD(u64 num_items,u64 seed = 0xdeadbeaf, double zip_const = kZipfianConst) :
      ZipFanD(0, num_items - 1, seed,zip_const) { }

  u64 next(u64 num) {
    assert(num >= 2 && num < kMaxNumItems);

    if (num > n_for_zeta_) { // Recompute zeta_n and eta
      raise_zeta(num);
      eta_ = eta();
    }

    double u = rand.next_uniform();
    double uz = u * zeta_n_;

    if (uz < 1.0) {
      return last_value_ = 0;
    }

    if (uz < 1.0 + std::pow(0.5, theta_)) {
      return last_value_ = 1;
    }

    return last_value_ = base_ + num * std::pow(eta_ * u - eta_ + 1, alpha_);
  }

  u64 next() {
    return next(num_items_);
  }

  u64 last() {
    return last_value_;
  }

 private:
  void raise_zeta(u64 num) {
    assert(num >= n_for_zeta_);
    zeta_n_ = zeta(n_for_zeta_, num, theta_, zeta_n_);
    n_for_zeta_ = num;
  }

  double eta() {
    return (1 - std::pow(2.0 / num_items_, 1 - theta_)) /
        (1 - zeta_2_ / zeta_n_);
  }

  static double zeta(u64 last_num, u64 cur_num,
                     double theta, double last_zeta) {
    double zeta = last_zeta;
    for (u64 i = last_num + 1; i <= cur_num; ++i) {
      zeta += 1 / std::pow(i, theta);
    }
    return zeta;
  }

  static double zeta(u64 num, double theta) {
    return zeta(0, num, theta, 0);
  }

  u64 num_items_;
  u64 base_; /// Min number of items to generate

  // Computed parameters for generating the distribution
  double theta_, zeta_n_, eta_, alpha_, zeta_2_;
  u64 n_for_zeta_; /// Number of items used to compute zeta_n
  u64 last_value_;

  r2::util::FastRandom rand;

};

}

} // end namespace fstore
