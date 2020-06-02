#pragma once

#include "../../../deps/r2/src/common.hh"

namespace xstore {

namespace xcomm {

namespace rw {

using namespace r2;

const u32 kInvalidSeq = 0;

#pragma pack(1)
template <typename T> struct alignas(64) WrappedType {
  // when the seq equals invalid seq, then it is being written
  volatile u32 seq = kInvalidSeq + 1;
  T payload;
  volatile u32 seq_check;
 public:
  WrappedType(const T &p) : payload(p), seq_check(seq) {}

  WrappedType() : seq_check(seq) {}

  T &get_payload() { return payload; }

  inline bool consistent() const {
    r2::compile_fence();
    return (this->seq == this->seq_check) && (this->seq != kInvalidSeq);
  }

  /*
    To perform an update on the object atomically,
    one must first call the before write, and then call the done write
   */
  inline void begin_write() {
    volatile u32 *sp = &(this->seq);
    *sp = kInvalidSeq;
    r2::compile_fence();
  }

  inline void done_write() {
    r2::compile_fence();
    volatile u32 *sp = &(this->seq);
    *sp = this->seq_check += 1;
    ASSERT(*sp != kInvalidSeq );
    this->seq_check = this->seq;
  }
};

} // namespace rw
} // namespace xcomm
} // namespace xstore
