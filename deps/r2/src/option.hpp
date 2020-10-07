#pragma once

#if __cplusplus > 201402L
#include <optional>
#else
/**
 * Disable warning.
 * Basically optional is very useful.
 */
//#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored "-W#warnings"
#include <experimental/optional>
//#pragma GCC diagnostic pop
#endif

namespace r2 {
#if __cplusplus > 201402L
template<typename T>
using Option = std::optional<T>;
#else
template<typename T>
using Option = std::experimental::optional<T>;
#endif
}