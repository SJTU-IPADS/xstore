#pragma once

#include <vector>

#include "../r2/src/common.hh"

namespace xstore {

using namespace r2;

// feature vector for the model, map a key (e.g., u64) -> [feature0, feature1,
// ...]
using FeatureV = std::vector<double>;

template <class Derived> struct KeyType {
  auto to_feature() -> FeatureV {
    return reinterpret_cast<Derived *>(this)->to_feature_impl();
  }

  auto from_u64(const u64 &k) {
    return reinterpret_cast<Derived *>(this)->from_u64_impl();
  }
};

// Default XStore key type
struct XKeyType : public KeyType<XKeyType> {
  const u64 d;

  XKeyType() = default;

  auto from_u64_impl(const u64 &k) {
    d = k;
  }

  auto to_feature_impl() -> FeatureV {
    auto ret = FeatureV();
    ret.push_back(static_cast<double>(d));
    return ret;
  }
};

static_assert(sizeof(XKeyType), sizeof(u64), "Not zero-cost abstraction");

} // namespace xstore
