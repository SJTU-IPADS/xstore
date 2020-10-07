#pragma once

#include <cstring>

#include "utils/spin_lock.hpp"

namespace fstore {

template<class Derived, typename T>
class AbstractData
{
public:
  utils::SpinLockWOP lock;
  void set_meta(const T& m) { static_cast<Derived*>(this)->set_meta_impl(m); }

  T get_meta() { return static_cast<Derived*>(this)->get_meta_impl(); }
};

#pragma pack(1)
template<int Payload>
struct OpaqueData : AbstractData<OpaqueData<Payload>, u64>
{
  static_assert(Payload >= 0 && Payload < 256,
                "Value in index has some limits in size");
  char data[Payload];

  void set_meta_impl(const u64& m)
  {
    u64* val = reinterpret_cast<u64*>(data);
    *val = m;
  }

  u64 get_meta_impl() { return *(reinterpret_cast<u64*>(data)); }

  OpaqueData<Payload>& operator=(const OpaqueData<Payload>& d)
  {
    memcpy(data, d.data, Payload);
    return *this;
  }

  static u32 get_payload()  {
    return Payload;
  }
};

} // fstore
