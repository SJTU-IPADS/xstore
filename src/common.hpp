#pragma once

#include <cinttypes>

#include "r2/src/common.hpp"
#include "r2/src/random.hpp"

#if __cplusplus > 201402L
#include <optional>
#else
/**
 * Disable warning.
 * Basically optional is very useful.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-W#warnings"
#include <experimental/optional>
#pragma GCC diagnostic pop
#endif

namespace fstore {

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef int64_t i64;
typedef int32_t i32;
typedef int8_t i8;
typedef uint8_t u8;
typedef unsigned int usize;

#if __cplusplus > 201402L
using Option = std::optional<T>;
#else
template<typename T>
using Option = std::experimental::optional<T>;
#endif

inline void
not_implemented()
{
  ASSERT(false) << "not implemented!";
}

const int CACHE_LINE_SZ = 128;

} // end namespace fstore
