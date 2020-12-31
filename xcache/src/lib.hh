#pragma once

#include <climits> // CHAR_BIT

//#include "../rmi_2.hh"

namespace xstore {

namespace xcache {

/**
 * Credicts: This nice code comes from
 * https://stackoverflow.com/questions/1392059/algorithm-to-generate-bit-mask
 */
// generate bitmasks
template <typename R> static constexpr R bitmask(unsigned int const onecount) {
  return static_cast<R>(-(onecount != 0)) &
         (static_cast<R>(-1) >> ((sizeof(R) * CHAR_BIT) - onecount));
}
} // namespace xcache

} // namespace xstore
