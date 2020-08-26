#pragma once

#include <unordered_map>

#include "../../../deps/r2/src/msg/ud_session.hh"
#include "../../../deps/rlib/core/lib.hh"
#include "../../../deps/rlib/core/qps/ud.hh"
#include "../../../deps/rlib/core/qps/recv_iter.hh"

#include "./trait.hh"

namespace xstore {

namespace transport {

using namespace rdmaio;
using namespace rdmaio::qp;

struct UDTransport : public STrait<UDTransport> {

  // session wraps the qp
  // TODO: may have memory leckage
  r2::UDSession *session = nullptr;

  // multiplex an existing QP
  // UD can multi-cast
  explicit UDTransport(UDSession *t) : session(t) {}

  UDTransport() = default;

  auto connect_impl(const std::string &addr, const std::string &s_name,
                    const u32 &my_id, Arc<UD> qp) -> Result<> {

    // avoid re-connect
    // re-connect must establish a new QP
    if (this->session != nullptr) {
      return ::rdmaio::Ok();
    }

    // try connect to the server
    ConnectManager cm(addr);
    auto wait_res = cm.wait_ready(1000000, 2);
    if (wait_res != IOCode::Ok) {
      return transfer_raw(wait_res);
    }

    // fetch the server addr
    auto fetch_qp_attr_res = cm.fetch_qp_attr(s_name);
    if (fetch_qp_attr_res != IOCode::Ok) {
      return transfer_raw(fetch_qp_attr_res);
    }

    auto ud_attr = std::get<1>(fetch_qp_attr_res.desc);

    this->session = new UDSession(my_id, qp, ud_attr);

    // TODO: send an extra connect message to the server
    return ::rdmaio::Ok();
  }

  auto send_impl(const MemBlock &msg, const double &timeout = 1000000)
      -> Result<std::string> {
    return session->send_pending(msg);
  }

  auto send_w_key_impl(const MemBlock &msg, const u32 &key,
                       const double &timeout = 1000000) -> Result<std::string> {
    return session->send_unsignaled(msg,key);
  }
};

// TODO: recv end point not implemented
template <usize es>
struct UDRecvTransport : public RTrait<UDRecvTransport, UDTransport> {

  std::unordered_map<u32, UDTransport> incoming_sessions;

  Arc<UD> qp;
  Arc<RecvEntries<es>> recv_entries;

  RecvIter<UD, es> iter;

  u32 cur_session_id = 0;

  UDRecvTransport(Arc<UD> qp, Arc<RecvEntries<es>> e)
      : qp(qp), recv_entries(e), iter(qp, e) {
  }

  void begin_impl() { iter.begin(); }

  void next_impl() { return iter.next(); }

  auto has_msgs_impl() -> bool { return iter.has_msgs(); }

  auto cur_msg_impl() -> MemBlock { return core.cur_msg(); }
};
}

// TODO: session manager impl



} // namespace xstore
