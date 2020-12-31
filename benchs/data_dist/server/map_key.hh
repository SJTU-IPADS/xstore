#pragma once

#include "../../../lib.hh"

namespace xstore {

const float max_lat = 360;
struct __attribute__((packed)) MapKey : public KeyType<MapKey>
{
  union
  {
    struct
    {
      float lat;
      float lon;
    } real;
    u64 d;
  } d;
  MapKey() = default;

  explicit MapKey(const u64& d) { this->from_u64(d); }

  MapKey(const float& lat, const float& lon)
  {
    this->d.real.lat = lat;
    this->d.real.lon = lon;
  }
  auto from_u64_impl(const u64& k)
  {
    // FIMEME: genereally, we donot allow init from u64 for this key
    *this = this->max();
  }

  auto to_u64_impl() -> u64 { return static_cast<double>(this->to_scalar()); }

  auto to_feature_impl() const -> FeatureV
  {
    auto ret = FeatureV();
    ret.push_back(this->d.real.lat);
    ret.push_back(this->d.real.lon);
    return ret;
  }

  auto to_feature_float_impl() const -> FeatureFV
  {
    auto ret = FeatureFV();
    ret.push_back(this->d.real.lat);
    ret.push_back(this->d.real.lon);
    return ret;
  }

  auto to_scalar_impl() const -> double
  {
    //    return this->d.d;
    return (this->d.real.lat + 360) * max_lat + (this->d.real.lon + 360);
  }

  static auto min() -> MapKey { return MapKey(-360, -360); }

  static auto max() -> MapKey
  {
    return MapKey(std::numeric_limits<float>::max() - 1,
                  std::numeric_limits<float>::max() - 1);
  }

  auto operator==(const MapKey& b) const -> bool
  {
    return this->to_scalar() == b.to_scalar();

    //    return this->d.d == b.d.d;
    return this->to_scalar() == b.to_scalar();
    return this->d.real.lon == b.d.real.lon && this->d.real.lat == b.d.real.lat;
  }

  auto operator>=(const MapKey& b) const -> bool
  {
    //    return *this == b || *this > b;
    return this->to_scalar() >= b.to_scalar();
  }

  auto operator!=(const MapKey& b) const -> bool
  {
    //    return this->d.d != b.d.d;
    return this->d.real.lat != b.d.real.lat || this->d.real.lon != b.d.real.lon;
  }

  auto operator<(const MapKey& b) const -> bool
  {
    return this->to_scalar() < b.to_scalar();
    //    return this->d.d < b.d.d;
    if (this->d.real.lat < b.d.real.lat) {
      return true;
    }
    if (this->d.real.lat == b.d.real.lat) {
      return this->d.real.lon < b.d.real.lon;
    }
    return false;
  }

  auto operator>(const MapKey& b) const -> bool
  {
    return this->to_scalar() > b.to_scalar();

    //    return this->d.d > b.d.d;
    if (this->d.real.lat > b.d.real.lat) {
      return true;
    }
    if (this->d.real.lat == b.d.real.lat) {
      return this->d.real.lon > b.d.real.lon;
    }
    return false;
  }

  void operator=(const MapKey& k)
  {
    //    return this->d.d == k.d.d;
    this->d.real.lon = k.d.real.lon;
    this->d.real.lat = k.d.real.lat;
  }

  friend std::ostream& operator<<(std::ostream& out, const MapKey& k)
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