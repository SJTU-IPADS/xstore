#pragma once

#include <unordered_map>

#include "./trait.hh"

#include "../../../deps/r2/src/ring_msg/mod.hh"

#include "../../../deps/rlib/core/qps/config.hh"

namespace xstore {

namespace transport {

using namespace rdmaio::qp;
using namespace r2::ring_msg;

/*!
  This file provides a wrapper of R2::ring_msg, making it implement the trait
  The r2_ring messages implements a simplfied version of FaRM message (FaRM@NSDI'14)
 */
template <usize R, usize kRingSz, usize kMaxMsg>
struct RRingTransport : public STrait<RRingTransport<R, kRingSz, kMaxMsg>> {
  using RingS = ::r2::ring_msg::Session<R, kRingSz, kMaxMsg>;
  using Self = RRingTransport<R, kRingSz, kMaxMsg>;
  /*!
    FixMe: currently the RingS may be passed in by another one.
    So we cannot free it directly in the deconstructor.
    So there would be a memory leakage.
    Maybe we can add an explict method to let user free it?
   */
  //Arc<RingS> core = nullptr;
  RingS *core = nullptr;

  // methods
  explicit RRingTransport(RingS *c) : core(c) {}

  RRingTransport(const u16 &id, Arc<RNic> nic, const QPConfig &config,
                 ibv_cq *cq, Arc<AbsRecvAllocator> alloc)
      : core(new RingS(id, nic, config, cq, alloc)) {}

  auto connect_impl(const std::string &host, const std::string &c_name,
                    const ::rdmaio::nic_id_t &nic_id,
                    const QPConfig &remote_qp_config) -> Result<> {
    RingCM cm(host);
    return core->connect(cm, c_name, nic_id, remote_qp_config);
  }

  auto send_impl(const MemBlock &msg, const double &timeout = 1000000)
      -> Result<std::string> {
    return core->send_unsignaled(msg);
  }

  auto send_w_key_impl(const MemBlock &msg, const u32 &key, const double &timeout = 1000000)
      -> Result<std::string> {
    return ::rdmaio::Err("not implemented!");
  }
};

/*!
  Wrapper to the recv section
 */
template <usize R, usize kRingSz, usize kMaxMsg>
struct RRingRecvTransport
    : public RTrait<RRingRecvTransport<R, kRingSz, kMaxMsg>,
                    RRingTransport<R, kRingSz, kMaxMsg>> {
  using ST = RRingTransport<R, kRingSz, kMaxMsg>;
  RingRecvIter<R, kRingSz, kMaxMsg> core;

  explicit RRingRecvTransport(Arc<Receiver<R, kRingSz, kMaxMsg>> &r)
      : core(r) {}

  void begin_impl() { core.begin(); }

  void next_impl() { return core.next(); }

  auto has_msgs_impl() -> bool { return core.has_msgs(); }

  auto cur_msg_impl() -> MemBlock { return core.cur_msg(); }

  auto reply_entry_impl() -> ST {
    return RRingTransport<R, kRingSz, kMaxMsg>(core.cur_session());
  }
};

} // namespace transport
} // namespace xstore
