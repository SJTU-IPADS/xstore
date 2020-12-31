#pragma once

#include "../../deps/r2/src/common.hh"

namespace xstore {

namespace xkv {

using namespace r2;

/*!
  interface of an xallocator
  each allocate unit is defined as N

  Note: we assumes that the interfaces are **thread safe**
*/
template <class Derived, usize N> class AllocTrait {
public:
  // interfaces
  auto alloc() -> ::r2::Option<char *> {
    return reinterpret_cast<Derived *>(this)->alloc_impl();
  }

  auto dealloc(char *data) {
    return reinterpret_cast<Derived *>(this)->dealloc_impl();
  }
};

/*!
  A trivial AllocTrait implementation using malloc/free
 */
template <usize S> class TrivalAlloc : public AllocTrait<TrivalAlloc<S>, S> {
public:
  auto alloc_impl() -> ::r2::Option<char *> {
    return reinterpret_cast<char *>(malloc(S));
  }

  auto dealloc_impl(char *data) { free(data); }
};

} // namespace xkv

} // namespace xstore
