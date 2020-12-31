#pragma once

#include "./async_rw_trait.hh"

#include "../../../deps/r2/src/rdma/async_op.hh"

namespace xstore {

namespace xcomm {

namespace rw {

using namespace r2::rdma;
using namespace rdmaio;

class AsyncRDMARWOp : public AsyncReadWriteTrait<AsyncRDMARWOp> {
  AsyncOp<1> op;
  Arc<RC> qp;
public:
  explicit AsyncRDMARWOp(Arc<RC> qp) : qp(qp) {}
  /*!
    In an RDMA context, the src buf ptr is a remote addr that can be
    passed tp r2::rdma::SROp. Basically, it is a wrapper over SROp,
    implemented ReadWriteTrait.
   */
  auto read_impl(const MemBlock &src, const MemBlock &dest, R2_ASYNC) -> Result<> {

    if (unlikely(src.sz > dest.sz)) {
      // size not match
      return ::rdmaio::Err();
    }
    op.set_rdma_addr(reinterpret_cast<u64>(src.mem_ptr), qp->remote_mr.value())
        .set_read()
        .set_payload(static_cast<const u64 *>(dest.mem_ptr), dest.sz,
                     qp->local_mr.value().lkey);

    auto ret = op.execute_async(this->qp, IBV_SEND_SIGNALED, R2_ASYNC_WAIT);
    return ::rdmaio::transfer_raw(ret);
  }
};
} // namespace rw
} // namespace xcomm
} // namespace xstore
