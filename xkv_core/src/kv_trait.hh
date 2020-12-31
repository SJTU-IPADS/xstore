#pragma once

#include "../../deps/r2/src/common.hh"

#include "./lib.hh"

namespace xstore {

namespace xkv {

using namespace r2;

template <class Derived, typename KeyType, typename V> class KVTrait {
 public:
  auto get(const KeyType &k) -> ::r2::Option<V> {
    return reinterpret_cast<Derived *>(this)->get_impl(k);
  }

  auto insert(const KeyType &k, const V &v) {
    return reinterpret_cast<Derived *>(this)->insert_impl(k,v);
  }
};

}
} // namespace xstore
