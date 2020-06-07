#pragma once

#include "../../../deps/r2/src/common.hh"
#include "./rw_trait.hh"

namespace xstore {

namespace xcomm {

namespace rw {

using namespace r2;

/*!
  The unwrapped type assumes that sizeof(T) <= 8, such that
  we can atomically read it wihtout using seq techniques.

  This class merely prevides similar interfaces as Wrappedtype
 */
template <typename T> struct __attribute__((packed)) UWrappedType {
  static_assert(sizeof(T) <= sizeof(u64), " T too large!");
  // when the seq equals invalid seq, then it is being written
  T payload;
  UWrappedType(const T &p) : payload(p) {}

  void init() {}

  /*!
    reset the content of the wrapped type
   */
  void reset(const T &t) {
    this->init();
    this->payload = t;
  }

  T &get_payload() { return payload; }

  // sz of the meta data, namely, two seqs
  static auto meta_sz() -> usize { return 0; }

  inline bool consistent() const { return true; }

  /*
   */
  inline void begin_write() {}

  inline void done_write() {}

} __attribute__((aligned(sizeof(u64))));

} // namespace rw
} // namespace xcomm
} // namespace xstore
