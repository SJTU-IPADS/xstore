#pragma once

#include "common.hpp"
#include "memory.hpp"
#include "util.hpp"

namespace rdmaio {

class UDQP : public QPDummy {
public:
  const RemoteMemory::Attr local_mem_;
  const RNic *rnic_p;
  const int max_send_size;

  int pending_reqs_ = 0;
  QPAttr attr;

  /*
    The following structures handles event handle model of RDMA
  */
  struct ibv_comp_channel *event_channel = nullptr;
  void *ev_ctx = nullptr;

public:
  UDQP(RNic &rnic, const RemoteMemory::Attr &local_mem, const QPConfig &config)
      : UDQP(rnic, local_mem, false, config) {}

  UDQP(RNic &rnic, const RemoteMemory::Attr &local_mem, bool with_channel,
       const QPConfig &config)
      : local_mem_(local_mem), rnic_p(&rnic),
        max_send_size(config.max_send_size),
        attr(rnic.addr, rnic.lid, config.rq_psn, rnic.id.port_id) {

    if (with_channel) {
      event_channel = ibv_create_comp_channel(rnic.ctx);
      if (!event_channel)
        return;
    }

    // create cq for send messages
    cq_ = QPUtily::create_cq(rnic, config.max_send_size);
    if (cq_ == nullptr)
      return;

    // create recv queue for receiving messages
    recv_cq_ =
        QPUtily::create_cq(rnic, config.max_recv_size, event_channel, ev_ctx);
    if (recv_cq_ == nullptr)
      return;

    qp_ = QPUtily::create_qp(IBV_QPT_UD, rnic, config, cq_, recv_cq_);

    /**
     * bring qp to the valid state: ready to recv + ready to send
     */
    if (qp_) {
      if (!bring_ud_to_recv(qp_) || !bring_ud_to_send(qp_, config.rq_psn)) {
        delete qp_;
        qp_ = nullptr;
      } else {
        // re-fill entries
        attr.qpn = qp_->qp_num;
        attr.qkey = config.qkey;
      }
    }
  }

  ~UDQP() {
    // QPUtily::destroy_qp(qp_);
  }

  /**
   * Some methods to help manage requests progress of this UD QP
   */
  inline int num_pendings() const { return pending_reqs_; }
  bool empty() const { return num_pendings() == 0; }

  inline bool need_poll(int threshold) const {
    return num_pendings() >= threshold;
  }

  inline bool need_poll() const { return need_poll(max_send_size / 2); }

  inline void forward(int num) { pending_reqs_ += 1; }
  inline void clear() { pending_reqs_ = 0; }

  QPAttr get_attr() const { return attr; }

  static bool bring_ud_to_recv(ibv_qp *qp) {
    int rc, flags = IBV_QP_STATE;
    struct ibv_qp_attr qp_attr = {};
    qp_attr.qp_state = IBV_QPS_RTR;

    rc = ibv_modify_qp(qp, &qp_attr, flags);
    return rc == 0;
  }

  static bool bring_ud_to_send(ibv_qp *qp, int psn) {
    int rc, flags = 0;
    struct ibv_qp_attr qp_attr = {};
    qp_attr.qp_state = IBV_QPS_RTS;
    qp_attr.sq_psn = psn;

    flags = IBV_QP_STATE | IBV_QP_SQ_PSN;
    rc = ibv_modify_qp(qp, &qp_attr, flags);
    return rc == 0;
  }
}; // end class UDQP

} // namespace rdmaio
