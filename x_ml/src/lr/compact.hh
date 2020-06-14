#pragma once

#include "./ml_trait.hh"
#include "./mod.hh"

namespace xstore {

namespace xml {

/*!
  LR in a compact form, which uses float (4-byte) to store ml parameters
 */
struct __attribute__((packed)) CompactLR : public MLTrait<CompactLR> {
  float w;
  float b;

  auto predict_impl(const u64 &key) -> double {
    return static_cast<double>(key) * w + b;
  }

  void train_impl(std::vector<u64> &train_data, std::vector<u64> &train_label,
                  int step) {
    LR lr;
    lr.train_impl(train_data, train_label, step);
    if (unlikely(static_cast<double>(std::numeric_limits<float>::max()) <=
                 lr.w)) {
      ASSERT(false) << "warning trained w too large: " << lr.w;
    }
    this->w = static_cast<float>(lr.w);
    if (unlikely(static_cast<double>(std::numeric_limits<float>::max()) <=
                 lr.b)) {
      ASSERT(false) << "warning trained b too large: " << lr.b;
    }

    this->b = static_cast<float>(lr.b);
  }

  auto serialize_impl() -> std::string {
    ASSERT(false) << "not implemented";
    return "";
  }

  void from_serialize_impl(const std::string &data) {
    ASSERT(false) << "not implemented";
  }
};

} // namespace xml

} // namespace xstore
