#pragma once

#include "../../../deps/r2/src/common.hh"
#include "./rw_trait.hh"

#include "./ptr.hh"

namespace xstore {

namespace xcomm {

namespace rw {

using namespace r2;

const u32 kInvalidSeq = 0;

#pragma pack(1)
template <typename T> struct alignas(64) WrappedType {
  // when the seq equals invalid seq, then it is being written
  volatile u64 seq = kInvalidSeq + 1;
  T payload;
  volatile u64 seq_check;
 public:
  WrappedType(const T &p) : payload(p), seq_check(seq) {}

  WrappedType() : seq_check(seq) {}

  T &get_payload() { return payload; }

  static auto meta_sz() -> usize {
    return sizeof(seq) + sizeof(seq_check);
  }

  inline bool consistent() const {
    r2::lfence();
    return this->seq == this->seq_check;
  }

  /*
    To perform an update on the object atomically,
    one must first call the before write, and then call the done write
   */
  inline void begin_write() {
    this->seq_check += 1;
    this->seq = kInvalidSeq;
    //::r2::store_fence();
    ::r2::compile_fence();
  }

  inline void done_write() {
    // the update of *seqs* should after previous writes
    //::r2::store_fence();
    ::r2::compile_fence();
    this->seq = this->seq_check;
    ::r2::compile_fence();
    //r2::store_fence();
  }
};

} // namespace rw
} // namespace xcomm
} // namespace xstore
