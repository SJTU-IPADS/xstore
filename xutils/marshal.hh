#pragma once

#include <string>

#if defined(__GNUC__) && __GNUC__ < 7
#include <experimental/string_view>
namespace xstore {
using string_view = ::std::experimental::string_view;
}
#else
#include <string_view>
namespace xstore {
using string_view = ::std::string_view;
}
#endif
#include <string.h>

#include "../deps/r2/src/common.hh"

namespace xstore {

namespace util {

using namespace r2;

/*!
  This file provides two utils class:
  - a class (Marshal) for writing an object with type (T) to a string, and
  vice verse
  - a class (MarshalT) for serializing an object, with size check by adding an
  header
 */

/*!
  Marshal converst a type (T) to string, and vice verse
 */
template<typename T>
class Marshal
{
public:
  /*!
    Simple helper function which write value to buffers
   */
  static inline auto serialize_to(const T& t) -> std::string
  {
    std::string data(sizeof(T), '\0');
    serialize_to(t, data);
    return data;
  }

  /*!
    \ret: buf + sizeof(T)
   */
  static inline auto unsafe_serialize(const T& t, char* buf) -> char*
  {
    memcpy(buf, &t, sizeof(T));
    return buf + sizeof(T);
  }

  static inline auto serialize_to(const T& t, std::string& s)
  {
    ASSERT(s.size() >= sizeof(T));
    memcpy((void*)s.data(), &t, sizeof(T));
  }

  static inline T deserialize(const char* buf, u64 size)
  {
    if (size < sizeof(T))
      ASSERT(false);
    T res;
    memcpy(&res, buf, sizeof(T));
    return res;
  }

  static inline T extract(const char* buf)
  {
    const T* ptr = reinterpret_cast<const T*>(buf);
    return *ptr;
  }

  static inline T extract_with_inc(char*& buf)
  {
    const T* ptr = reinterpret_cast<const T*>(buf);
    buf += sizeof(T);
    return *ptr;
  }

  static inline ::r2::Option<T> deserialize_opt(const char* buf, u64 size)
  {
    if (size < sizeof(T)) {
      return {};
    }
    T res;
    memcpy(&res, buf, sizeof(T));
    return res;
  }
};

/*!
  Here is the protocol:
  - | size of T (u64) | an opaque content |
 */
template<typename T>
class MarshalT
{
public:
  struct __attribute__((packed)) _WrapperT
  {
    u64 sz;
    T payload;
  };

  static inline auto serialize(const T& t) -> std::string
  {
    _WrapperT temp = {
      .sz = sizeof(T),
      .payload = t,
    };
    return Marshal<_WrapperT>::serialize_to(temp);
  }

  static inline auto deserialize(const std::string& d) -> ::r2::Option<T>
  {
    auto res = Marshal<_WrapperT>::deserialize_opt(d.data(), d.size());
    if (res) {
      return res.value().payload;
    }
    return {};
  }

  static inline auto deserialize(const string_view& d) -> ::r2::Option<T>
  {
    auto res = Marshal<_WrapperT>::deserialize_opt(d.data(), d.size());
    if (res) {
      return res.value().payload;
    }
    return {};
  }
};

} // namespace util
} // namespace xstore
