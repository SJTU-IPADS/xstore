#pragma once

#include "./trait.hh"
#include "../../../deps/r2/src/ring_msg/mod.hh"

#include "../../../deps/rlib/core/qps/config.hh"

namespace xstore {

namespace transport {

using namespace rdmaio::qp;
using namespace r2::ring_msg;

/*!
  This file provides a wrapper of R2::ring_msg, making it implement the trait
 */
template <usize R, usize kRingSz, usize kMaxMsg>
struct RRingTransport : public STrait<RRingTransport<R,kRingSz,kMaxMsg>> {
  ::r2::ring_msg::Session<R,kRingSz,kMaxMsg> core;

  RRingTransport(const u16 &id, Arc<RNic> nic, const QPConfig &config,
                 ibv_cq *cq, Arc<AbsRecvAllocator> alloc)
      : core(id, nic, config, cq, alloc) {}

  auto connect_impl(const std::string &host, const std::string &c_name,
                    const ::rdmaio::nic_id_t &nic_id,
                    const QPConfig &remote_qp_config) -> Result<> {
    RingCM cm(host);
    return core.connect(cm, c_name, nic_id, remote_qp_config);
  }

  auto send_impl(const MemBlock &msg, const double &timeout = 1000000)
      -> Result<std::string> {
    return core.send_unsignaled(msg);
  }
};

} // namespace transport
} // namespace xstore
