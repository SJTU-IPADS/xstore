#pragma once

#include "../../../lib.hh"

#include <assert.h>

namespace xstore {

const usize kMaxDPerW = 10;
const usize kMaxOrderPD = 200000;

const usize kMaxOL = 4000;

struct __attribute__((packed)) TPCCKey : public KeyType<TPCCKey>
{
  u64 d;

  TPCCKey() = default;

  TPCCKey(const usize& w, const usize& d, const usize& o, const usize& n)
  {
    u64 upper = w * kMaxDPerW + d;
    ASSERT(upper < kMaxOrderPD);
    upper = upper * kMaxOrderPD + o;
    this->d = (upper << 32) + n;
  }
  explicit TPCCKey(const u64& d)
    : d(d)
  {}
  auto from_u64_impl(const u64& k) { this->d = k; }

  auto to_u64_impl() -> u64 { return d; }

  auto to_feature_impl() const -> FeatureV
  {
    auto ret = FeatureV();
    auto n = (this->d & 0xffffffff) % kMaxOL;
    auto upper = this->d >> 32;
    auto o = upper % kMaxOrderPD;
    upper = upper / kMaxOrderPD;
    auto d = upper % kMaxDPerW;

    ret.push_back(upper / kMaxDPerW);
    ret.push_back(d);
    ret.push_back(o);
    ret.push_back(n);
    ASSERT(TPCCKey(upper / kMaxDPerW, d, o, n).to_scalar() == this->d)
      << "decode: " << upper / kMaxDPerW << " " << d << " " << o << " " << n;
    return ret;
  }

  auto to_scalar_impl() const -> double { return static_cast<double>(this->d); }

  static auto min() -> TPCCKey { return TPCCKey(0); }

  static auto max() -> TPCCKey
  {
    return TPCCKey(std::numeric_limits<u64>::max());
  }

  void operator=(const TPCCKey& k) { this->d = k.d; }
  auto operator==(const TPCCKey& b) const -> bool { return this->d == b.d; }

  auto operator>=(const TPCCKey& b) const -> bool { return this->d >= b.d; }

  auto operator!=(const TPCCKey& b) const -> bool { return this->d != b.d; }

  auto operator<(const TPCCKey& b) const -> bool { return this->d < b.d; }

  auto operator>(const TPCCKey& b) const -> bool { return this->d > b.d; }

  friend std::ostream& operator<<(std::ostream& out, const TPCCKey& k)
  {
    auto feature = k.to_feature();
    out << "[";
    for (auto f : feature) {
      out << f << ",";
    }
    return out << "]";
  }
};

static_assert(sizeof(TPCCKey) == sizeof(u64),
              "This TPC-C key should align to 8B");

}
