#pragma once

#include <cinttypes>

#include "macros.hpp"
#include "logging.hpp"
#include "option.hpp"

namespace r2 {

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef int64_t i64;
typedef uint8_t u8;
typedef int8_t i8;
typedef unsigned int usize;

constexpr usize kCacheLineSize = 128;

} // end namespace r2
