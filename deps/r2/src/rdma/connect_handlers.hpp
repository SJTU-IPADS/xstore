#pragma once

#include "../common.hpp"
#include "rlib/rdma_ctrl.hpp"

#include <functional>
#include <mutex>

namespace r2 {

namespace rdma {

using namespace rdmaio;

enum
{
  CreateConnect = (::rdmaio::RESERVED_REQ_ID::FREE + 1),
};


class NicRegister {
  std::mutex guard;
  std::map<u64,RNic *> store;
public:
  NicRegister() = default;

  RNic* reg(u64 id, RNic* n)
  {
    std::lock_guard<std::mutex> lk(guard);
    if (store.find(id) != store.end())
      return store[id];
    store.insert(std::make_pair(id, n));
    return n;
  }

  RNic *get(u64 id) {
    std::lock_guard<std::mutex> lk(guard);
    if (store.find(id) != store.end())
      return store[id];
    return nullptr;
  }

  RNic* dereg(u64 id)
  {
    std::lock_guard<std::mutex> lk(guard);
    auto it = store.find(id);
    if (it != store.end()) {
      auto res = it->second;
      store.erase(it);
      return res;
    }
    return nullptr;
  }
};

/*!
    RDMAHandlers register callback to RdmaCtrl, so that:
    it handles QP creation requests;
    it handles QP deletion requests, etc.
 */
class ConnectHandlers
{
public:
  struct CCReq
  {
    u64 qp_id;
    u64 nic_id;
    QPAttr attr;
    QPConfig config;
  };

  struct __attribute__ ((packed)) CCReply
  {
    IOStatus res;
    QPAttr attr;
  };
  /*!
    This handler receive a CCReq, create a corresponding QP,
    register it with RdmaCtrl, and finally return the created QP's attr.
    If creation failes, then a {} is returned.
   */
  static std::tuple<IOStatus, QPAttr> create_connect(const CCReq& req,
                                                     RdmaCtrl& ctrl,
                                                     //std::vector<RNic*>& nics)
                                                     NicRegister &nics)
  {
    auto ret = std::make_pair(WRONG_ARG, QPAttr());
    // create and reconnect
    RemoteMemory::Attr dummy; // since we are creating QP for remote, no MR attr
    // is required for it
    auto rnic = nics.get(req.nic_id);
    if (rnic == nullptr) {
      ASSERT(false);
      return ret;
    }
    auto& nic = *rnic;
    auto qp = new RCQP(nic, dummy, dummy, req.config);
    ASSERT(qp != nullptr);

    // try register the QP. since a QP cannot be re-connected, so
    // we return SUCC with the already create QP's result
    if (!ctrl.qp_factory.register_rc_qp(req.qp_id, qp)) {
      goto DUPLICATE_RET;
    }
    // we connect for the qp
    {
      auto code = qp->connect(req.attr, req.config);

      if (code != SUCC) {
        std::get<0>(ret) = code;
        ctrl.qp_factory.delete_rc_qp(req.qp_id);
        goto ERR_RET;
      }
    }
    return std::make_pair(SUCC, qp->get_attr());
  DUPLICATE_RET:
    ret =
      std::make_pair(SUCC, ctrl.qp_factory.get_rc_qp(req.qp_id)->get_attr());
  ERR_RET:
    delete qp;
    return ret;
  }

  static bool register_cc_handler(RdmaCtrl& ctrl, NicRegister& nics)
  {
    ctrl.register_handler(CreateConnect,
                          std::bind(ConnectHandlers::create_connect_wrapper,
                                    std::ref(ctrl),
                                    std::ref(nics),
                                    std::placeholders::_1));
  }

private:
  static Buf_t create_connect_wrapper(RdmaCtrl& ctrl,
                                      NicRegister &nics,
                                      const Buf_t& req)
  {
    if (req.size() < sizeof(CCReq))
      return Marshal::null_reply();
    auto decoded_req = Marshal::deserialize<CCReq>(req);

    auto res = create_connect(decoded_req, ctrl, nics);

    CCReply reply;

    if (std::get<0>(res) == SUCC) {
      reply.res = SUCC;
      reply.attr = std::get<1>(res);
    } else {
      reply.res = std::get<0>(res);
    }
    return Marshal::serialize_to_buf(reply);
  }
};
} // namespace rdma

} // namespace r2
