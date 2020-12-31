#pragma once

#include "./mod.hh"

namespace xstore {

namespace xml {

/*!
  LR in a compact form, which uses float (4-byte) to store ml parameters
 */
template <typename Key>
struct __attribute__((packed)) CompactLR : public MLTrait<CompactLR<Key>,Key> {
  float w;
  float b;

  auto predict_impl(const Key &key) -> double {
    auto feature = key.to_feature();
    return feature.at(0) * w + b;
  }

  void train_impl(std::vector<Key> &train_data, std::vector<u64> &train_label,
                  int step) {
    LR<XKey> lr;
    // leverage
    lr.train(train_data, train_label, step);
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
