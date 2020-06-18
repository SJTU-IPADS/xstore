#pragma once

#include "./iter_trait.hh"
#include "./xarray.hh"

namespace xstore {

namespace xkv {

/*!
  Implements the key iterate trait to XArray
 */
struct ArrayIter : public KeyIterTrait<ArrayIter> {};
} // namespace xkv

} // namespace xstore
