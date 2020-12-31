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
    auto wait_res = cm.wait_ready(1000000, 4);
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

    return ::rdmaio::Ok();
  }

  auto send_impl(const MemBlock &msg, const double &timeout = 1000000)
      -> Result<std::string> {
    return session->send_unsignaled(msg);
  }

  auto get_connect_data_impl() -> r2::Option<std::string> {
    if (this->session) {
      std::string ret(sizeof(QPAttr),'0');
      auto attr = this->session->ud->my_attr();
      memcpy((void *)ret.data(), &attr, sizeof(QPAttr));
      return ret;
    }
    return {};
  }

  auto send_w_key_impl(const MemBlock &msg, const u32 &key,
                       const double &timeout = 1000000) -> Result<std::string> {
    return session->send_unsignaled(msg,key);
  }
};

// TODO: recv end point not implemented
template <usize es>
struct UDRecvTransport : public RTrait<UDRecvTransport<es>, UDTransport> {

  Arc<UD> qp;
  Arc<RecvEntries<es>> recv_entries;

  RecvIter<UD, es> iter;

  UDRecvTransport(Arc<UD> qp, Arc<RecvEntries<es>> e)
      : qp(qp), recv_entries(e) {
    // manally set the entries
    iter.set_meta(qp, recv_entries);
  }

  void begin_impl() {
    iter.begin(qp, recv_entries->wcs);
  }

  void end_impl() {
    // post recvs
    iter.clear();
  }

  void next_impl() {
    return iter.next();
  }

  auto has_msgs_impl() -> bool { return iter.has_msgs(); }

  auto cur_session_id_impl() -> u32 {
    return std::get<0>(iter.cur_msg().value());
  }

  // 4000: a UD packet can store at most 4K bytes
  auto cur_msg_impl() -> MemBlock {
    return MemBlock(
        reinterpret_cast<char *>(std::get<1>(iter.cur_msg().value())) + kGRHSz,
        4000);
  }
};

template <usize es>
struct UDSessionManager : public SessionManager<UDSessionManager<es>, UDTransport,
                                                UDRecvTransport<es>> {
  // assumption: the id is not in the incoming_sessions
  auto add_impl(const u32 &id, const MemBlock &raw_connect_data, UDRecvTransport<es> &recv_trait) -> Result<> {
    // should parse this as a UDPtr
    auto attr_ptr = raw_connect_data.interpret_as<QPAttr>(0);
    ASSERT(attr_ptr != nullptr);

    auto transport =
        std::make_unique<UDTransport>(new UDSession(id, recv_trait.qp, *attr_ptr));

    this->incoming_sesions.insert(std::make_pair(id, std::move(transport)));

    return ::rdmaio::Ok();
  }
};

}

} // namespace xstore
