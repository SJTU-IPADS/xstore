#pragma once

#include "./compact.hh"

namespace xstore {

namespace xml {

/*!
  Unlike LR, MLR(multi-variate LR) would predict a value from [0:base)
 */
// BT states for BaseType
template <typename BT = u32, typename LRT = CompactLR>
struct __attribute__((packed)) MLR : public MLTrait<MLR<BT,LRT>> {
  LRT       lr;
  BT        base;

  void set_base(const BT &b) {
    this->base = b;
  }

  auto predict_impl(const u64 &key) -> double {
    auto res = this->lr.predict_impl(key);
    if (res < 0) {
      res = 0;
    } else if (res > base) {
      res = base;
    }
    return res;
  }

  void train_impl(std::vector<u64> &train_data, std::vector<u64> &train_label,
                  int step) {
    this->lr.train_impl(train_data, train_label,step);
  }

  auto serialize_impl() -> std::string {
    ASSERT(false) << "not implemented";
    return "";
  }

  void from_serialize_impl(const std::string &data) {
    ASSERT(false) << "not implemented";
  }
};

}
} // namespace xstore
