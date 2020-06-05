#pragma once

#include "../deps/r2/src/common.hh"

namespace xstore {

namespace xkv {

using namespace r2;

template <class Derived, typename V> class KVTrait {
 public:
  auto get(const u64 &k) -> Option<V> {
    return reinterpret_cast<Derived *>(this)->get_impl(k);
  }

  auto insert(const u64 &k, const V &v) {
    return reinterpret_cast<Derived *>(this)->insert_impl(k,v);
  }
};

}
} // namespace xstore
