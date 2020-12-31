#pragma once

#include "../../../deps/r2/src/common.hh"
#include "./rw_trait.hh"

#include "./ptr.hh"

namespace xstore {

namespace xcomm {

namespace rw {

using namespace r2;

const u32 kInvalidSeq = 0;

template <typename T> struct __attribute__((packed)) WrappedType {
  // when the seq equals invalid seq, then it is being written
  volatile u32 seq = kInvalidSeq + 1;
  T payload;
  volatile u32 seq_check;

 public:
  WrappedType() = default;

  WrappedType(const T &p) : payload(p) {
    this->init();
  }

  void init() {
    this->seq = kInvalidSeq + 1;
    this->seq_check = this->seq;
  }

  /*!
    reset the content of the wrapped type
   */
  void reset(const T &t) {
    this->init();
    this->payload = t;
  }

  T &get_payload() { return payload; }

  T *get_payload_ptr() { return &payload; }

  // sz of the meta data, namely, two seqs
  static auto meta_sz() -> usize {
    return sizeof(seq) + sizeof(seq_check);
  }

  inline bool consistent() const {
    return (this->seq == this->seq_check) && (this->seq != kInvalidSeq);
  }

  /*
    To perform an update on the object atomically,
    one must first call the before write, and then call the done write
   */
  inline auto begin_write() -> u32 {
    auto ret = this->seq_check;
    this->seq_check = kInvalidSeq;
    this->seq = kInvalidSeq;
    ::r2::compile_fence();
    return ret;
  }

  // seq must be the return value of begin_write
  inline void done_write(const u32 &seq) {
    // the update of *seqs* should after previous writes
    ::r2::compile_fence();
    this->seq = seq + 1;
    this->seq_check = this->seq;
    ::r2::compile_fence();
  }
} __attribute__((aligned(64)));

} // namespace rw
} // namespace xcomm
} // namespace xstore
