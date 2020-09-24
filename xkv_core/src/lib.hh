#pragma once

#include <limits.h>

#include "../../lib.hh"

namespace xstore {

namespace xkv {

template <typename R> static inline constexpr R bitmask(unsigned int const onecount) {
  return static_cast<R>(-(onecount != 0)) &
         (static_cast<R>(-1) >> ((sizeof(R) * CHAR_BIT) - onecount));
}

const usize kValBit = 16;

/*!
  A fat pointer which encodes | val_ptr | val_sz |
  We use 48-bit to encode the pointer
 */
struct __attribute__((packed))  FatPointer {
  u64 encoded_val;

  template <typename T>
  explicit FatPointer (T *real_ptr, usize sz) {
    u64 real_ptr_v = reinterpret_cast<u64>(real_ptr);
    ASSERT(real_ptr_v < (1L << (64 - kValBit)))
        << real_ptr << " " << 1L << (64 - kValBit);
    ASSERT(sz < 1 << kValBit);
    encoded_val = (real_ptr_v << kValBit) | (sz);
  }

  explicit FatPointer(const u64 &v) : encoded_val(v) {}

  auto as_u64() -> u64 { return encoded_val; }

  template <typename T>
  auto get_ptr() -> T * {
    auto real_ptr_v = encoded_val >> kValBit;
    return  reinterpret_cast<T *>(real_ptr_v);
  }

  auto get_sz() -> usize {
    auto mask = bitmask<u64>(kValBit);
    return static_cast<usize>(encoded_val & mask);
  }
};

static_assert(sizeof(FatPointer) == sizeof(u64), "");

} // namespace xkv
} // namespace xstore
