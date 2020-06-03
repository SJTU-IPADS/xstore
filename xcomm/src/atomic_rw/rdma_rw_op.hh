#pragma once

#include "./rw_trait.hh"

#include "../../../deps/r2/src/rdma/sop.hh"

namespace xstore {

namespace xcomm {

namespace rw {

class RDMARWOp : public ReadWriteTrait<RDMARWOp> {
  SROp op;
  const Arc<RC> qp;
public:
  /*!
    In an RDMA context, the src buf ptr is a remote addr that can be
    passed tp r2::rdma::SROp. Basically, it is a wrapper over SROp,
    implemented ReadWriteTrait.
   */
  auto read_impl(const MemBlock &src, const MemBlock &dest) -> Result<> {
    return ::rdmaio::Ok();
  }
};
} // namespace rw
} // namespace xcomm
} // namespace xstore
