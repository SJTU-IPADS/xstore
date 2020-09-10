#pragma once

#include <vector>

#include "../r2/src/common.hh"

namespace xstore {

using namespace r2;

// feature vector for the model, map a key (e.g., u64) -> [feature0, feature1,
// ...]
using FeatureV = std::vector<double>;

template <class Derived> struct KeyType {
  auto to_feature() const -> FeatureV {
    return reinterpret_cast<const Derived *>(this)->to_feature_impl();
  }

  auto from_u64(const u64 &k) {
    return reinterpret_cast<Derived *>(this)->from_u64_impl();
  }

  auto feature_sz() -> usize {
    return this->to_feature().size();
  }
};

// Default XStore key type
struct __attribute__((packed))  XKey : public KeyType<XKey> {
  u64 d;

  XKey() = default;

  explicit XKey(const u64 &k) : d(k) {
  }

  auto from_u64_impl(const u64 &k) {
    this->d = k;
  }

  auto to_feature_impl() const -> FeatureV {
    auto ret = FeatureV();
    ret.push_back(static_cast<double>(d));
    return ret;
  }
};

static_assert(sizeof(XKey) == sizeof(u64), "Not zero-cost abstraction");

} // namespace xstore
