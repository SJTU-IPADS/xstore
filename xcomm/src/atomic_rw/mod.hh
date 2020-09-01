#pragma once

#include "./rw_trait.hh"
#include "./wrapper_type.hh"

namespace xstore {

namespace xcomm {

namespace rw {

// OP must implement ReadWriteTrait
class AtomicRW {
  usize retry_cnt = 0;

public:
  AtomicRW() = default;

  auto report_reties() -> usize { return this->retry_cnt; }

  template <typename T, class OP>
  auto atomic_read(OP &op, MemBlock &src, MemBlock &dest) -> Result<> {
    // 1. sanity check the buffer
    if (dest.sz < sizeof(WrappedType<T>)) {
      ASSERT(false);
      return ::rdmaio::Err();
    }
    dest.sz = sizeof(WrappedType<T>);

  retry:
    this->retry_cnt = 0;
    // 1. read the object
    auto res = op.read(src, dest);
    if (unlikely(res != ::rdmaio::IOCode::Ok)) {
      return res;
    }

    // 2. check consistent
    r2::compile_fence();
    // check consistency
    WrappedType<T> *v_p = reinterpret_cast<WrappedType<T> *>(dest.mem_ptr);
    if (unlikely(!v_p->consistent())) {
      // if (vs->seq != pseq) {
      r2::relax_fence();
      this->retry_cnt += 1;
      if (unlikely(this->retry_cnt > 1000000)) {
        ASSERT(false) << "should never rety so much:" << retry_cnt
                      << "; seqs: " << v_p->seq << " " << v_p->seq_check;
      }
      goto retry;
    }
    return res;
  }
};

} // namespace rw
} // namespace xcomm
} // namespace xstore
