#pragma once

#include <iostream>
#include <limits>
#include <vector>

#include <memory>

#include "deps/r2/src/common.hh"

namespace xstore {

using namespace r2;
template<typename T>
using Arc = std::shared_ptr<T>;

// feature vector for the model, map a key (e.g., u64) -> [feature0, feature1,
// ...]
using FeatureV = std::vector<double>;
using FeatureFV = std::vector<float>;

template<class Derived>
struct KeyType
{
  auto to_feature() const -> FeatureV
  {
    return reinterpret_cast<const Derived*>(this)->to_feature_impl();
  }

  auto to_feature_float() const -> FeatureFV
  {
    return reinterpret_cast<const Derived*>(this)->to_feature_float_impl();
  }

  auto to_scalar() const -> double
  {
    return reinterpret_cast<const Derived*>(this)->to_scalar_impl();
  }

  void from_u64(const u64& k)
  {
    reinterpret_cast<Derived*>(this)->from_u64_impl(k);
  }

  auto to_u64() -> u64
  {
    return reinterpret_cast<Derived*>(this)->to_u64_impl();
  }

  static auto min() -> KeyType { return Derived::min(); }

  static auto max() -> KeyType { return Derived::max(); }

  auto feature_sz() -> usize { return this->to_feature().size(); }
};

// Default XStore key type
struct __attribute__((packed)) XKey : public KeyType<XKey>
{
  u64 d;

  XKey() = default;

  explicit XKey(const u64& k)
    : d(k)
  {}

  auto from_u64_impl(const u64& k) { this->d = k; }

  auto to_u64_impl() -> u64 { return d; }

  auto to_scalar_impl() const -> u64 { return d; }

  auto to_feature_impl() const -> FeatureV
  {
    auto ret = FeatureV();
    ret.push_back(static_cast<double>(d));
    return ret;
  }

  auto to_feature_float_impl() const -> FeatureFV
  {
    auto ret = FeatureFV();
    ret.push_back(static_cast<float>(d));
    return ret;
  }

  static auto min() -> XKey { return XKey(0); }

  static auto max() -> XKey { return XKey(std::numeric_limits<u64>::max()); }

  auto operator==(const XKey& b) const -> bool { return this->d == b.d; }

  auto operator>=(const XKey& b) const -> bool { return this->d >= b.d; }

  auto operator<=(const XKey& b) const -> bool { return this->d <= b.d; }

  auto operator!=(const XKey& b) const -> bool { return this->d != b.d; }

  auto operator<(const XKey& b) const -> bool { return this->d < b.d; }

  auto operator>(const XKey& b) const -> bool { return this->d > b.d; }

  friend std::ostream& operator<<(std::ostream& out, const XKey& k)
  {
    return out << "[" << k.d << "]";
  }
};

static_assert(sizeof(XKey) == sizeof(u64), "Not zero-cost abstraction");

} // namespace xstore
