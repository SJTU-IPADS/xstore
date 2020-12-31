#pragma once

#include "../../lib.hh"

namespace test {

using namespace xstore;

const usize kMaxDPerW = 10;
const usize kMaxOrderPD = 100000;

const usize kMaxOL = 15;

using namespace xstore;
struct __attribute__((packed)) TestKey : public KeyType<TestKey>
{
  u64 d;

  TestKey() = default;

  TestKey(const usize& w, const usize& d, const usize& o, const usize& n)
  {
    auto upper = w * kMaxDPerW + d;
    upper = upper * kMaxOrderPD + o;
    this->d = upper * kMaxOL + n;
  }
  explicit TestKey(const u64& d)
    : d(d)
  {}
  auto from_u64_impl(const u64& k) { this->d = k; }

  auto to_u64_impl() -> u64 { return d; }

  auto to_scalar_impl() -> u64 { return d; }
  auto to_feature_impl() const -> FeatureV
  {
    auto ret = FeatureV();
    auto n = this->d % kMaxOL;
    auto upper = this->d / kMaxOL;
    auto o = upper % kMaxOrderPD;
    upper = upper / kMaxOrderPD;
    auto d = upper % kMaxDPerW;

    ret.push_back(upper / kMaxDPerW);
    ret.push_back(d);
    ret.push_back(o);
    ret.push_back(n);
    return ret;
  }

  static auto min() -> TestKey { return TestKey(0); }

  static auto max() -> TestKey
  {
    return TestKey(std::numeric_limits<u64>::max());
  }

  auto operator==(const TestKey& b) const -> bool { return this->d == b.d; }

  auto operator>=(const TestKey& b) const -> bool { return this->d >= b.d; }

  auto operator!=(const TestKey& b) const -> bool { return this->d != b.d; }

  auto operator<(const TestKey& b) const -> bool { return this->d < b.d; }

  auto operator>(const TestKey& b) const -> bool { return this->d > b.d; }

  friend std::ostream& operator<<(std::ostream& out, const TestKey& k)
  {
    return out << k.d;
  }
};
}