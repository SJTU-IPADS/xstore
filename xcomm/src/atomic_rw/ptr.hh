#pragma once

namespace xstore {

namespace xcomm {

namespace rw {

template <typename T> inline auto read_volatile(char *ptr) -> T {
  volatile T *v_ptr = reinterpret_cast<volatile T *>(ptr);
  return *v_ptr;
}

template <typename T> inline void write_volatile(char *ptr, const T &v) {
  volatile T *v_ptr = reinterpret_cast<volatile T *>(ptr);
  *v_ptr = v;
}

} // namespace rw
} // namespace xcomm
} // namespace xstore
