#pragma once

#include "./rw_trait.hh"

#include "../../../deps/r2/src/rdma/sop.hh"
#include "../../../deps/rlib/core/qps/rc.hh"

namespace xstore {

namespace xcomm {

namespace rw {

using namespace r2::rdma;
using namespace rdmaio;

class RDMARWOp : public ReadWriteTrait<RDMARWOp> {
  SROp op;
  Arc<RC> qp;
public:
  explicit RDMARWOp(Arc<RC> qp) : qp(qp) {}
  /*!
    In an RDMA context, the src buf ptr is a remote addr that can be
    passed tp r2::rdma::SROp. Basically, it is a wrapper over SROp,
    implemented ReadWriteTrait.
   */
  auto read_impl(const MemBlock &src, const MemBlock &dest) -> Result<> {

    if (src.sz > dest.sz) {
      // size not match
      return ::rdmaio::Err();
    }

    // transfer to a Wrapper version of SROp
    this->op.set_read()
        .set_remote_addr(reinterpret_cast<u64>(src.mem_ptr))
        .set_payload(dest.mem_ptr, dest.sz);
    auto ret = op.execute_sync(this->qp, IBV_SEND_SIGNALED);
    return ::rdmaio::transfer_raw(ret);
  }
};
} // namespace rw
} // namespace xcomm
} // namespace xstore
