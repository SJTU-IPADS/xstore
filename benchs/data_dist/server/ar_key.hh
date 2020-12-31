#pragma once

#include "../../lib.hh"

namespace xstore {

struct __attribute__((packed)) _internal
{
  u64 p_id : 32;
  u64 r_id : 32;
};
struct __attribute__((packed)) ARKey : public KeyType<ARKey>
{
  union
  {
    _internal d;
    u64 pad;
  } d;
  ARKey() = default;
  ARKey(const u64& p, const u64& r)
  {
    d.d.p_id = p;
    d.d.r_id = r;
  }

  explicit ARKey(const u64& d) { this->from_u64(d); }

  auto from_u64_impl(const u64& k) { this->d.pad = k; }

  auto to_u64_impl() -> u64 { return d.pad; }

  auto to_feature_impl() const -> FeatureV
  {
    auto ret = FeatureV();
    ret.push_back(d.d.p_id);
    ret.push_back(d.d.r_id);
    return ret;
  }

  auto to_scalar_impl() const -> double
  {
    return static_cast<double>(this->d.pad);
  }

  static auto min() -> ARKey { return ARKey(0); }

  static auto max() -> ARKey { return ARKey(std::numeric_limits<u64>::max()); }

  auto operator==(const ARKey& b) const -> bool
  {
    return this->d.pad == b.d.pad;
  }

  auto operator>=(const ARKey& b) const -> bool
  {
    return this->d.pad >= b.d.pad;
  }

  auto operator!=(const ARKey& b) const -> bool
  {
    return this->d.pad != b.d.pad;
  }

  auto operator<(const ARKey& b) const -> bool { return this->d.pad < b.d.pad; }

  auto operator>(const ARKey& b) const -> bool { return this->d.pad > b.d.pad; }

  friend std::ostream& operator<<(std::ostream& out, const ARKey& k)
  {
    auto feature = k.to_feature();
    out << "[";
    for (auto f : feature) {
      out << f << ",";
    }
    return out << "]";
  }
};

}