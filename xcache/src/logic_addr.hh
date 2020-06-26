#pragma once

#include <climits> // CHAR_BIT

#include "../../deps/r2/src/common.hh"

namespace xstore {

namespace xcache {

using namespace r2;

/*!
  The logic addr is in the form as:
  | -- ID bits -- | -- offset bits --| (total 8 bytes)

  The LogicAddr class will help manage the translation, encoding and decoding
  of address.

  The unit test file is in ../tests/test_logic.cc
 */

const usize id_bits = 32;
const usize off_bits = 32;

/**
 * Credicts: This nice code comes from
 * https://stackoverflow.com/questions/1392059/algorithm-to-generate-bit-mask
 */
// generate bitmasks
template <typename R> static constexpr R bitmask(unsigned int const onecount) {
  return static_cast<R>(-(onecount != 0)) &
         (static_cast<R>(-1) >> ((sizeof(R) * CHAR_BIT) - onecount));
}

struct LogicAddr {

  static auto encode_logic_addr(const u64 &id, const u64 &off) -> u64 {
    return (id << id_bits) | off;
  }

  static auto decode_logic_id(const u64 &encoded) -> u64 {
    return encoded >> 32;
  }

  static auto decode_off(const u64 &encoded) -> u64 {
    auto msk = bitmask<u64>(off_bits);
    return encoded & msk;
  }
};

} // namespace xcache
} // namespace xstore
