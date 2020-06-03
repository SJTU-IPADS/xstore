#pragma once

#include "../deps/r2/src/common.hh"

namespace xstore {

namespace xkv {

using namespace r2;

// interface of an xallocator
template <class Derived> class AllocTrait {
public:
  // interfaces
  auto alloc() -> Option<char *> {
    return reinterpret_cast<Derived *>(this)->alloc_impl();
  }

  auto dealloc(char *data) {
    return reinterpret_cast<Derived *>(this)->dealloc_impl();
  }
};
} // namespace xkv

} // namespace xstore
