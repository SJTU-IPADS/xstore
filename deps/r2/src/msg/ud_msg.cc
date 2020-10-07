#include "rlib/rdma_ctrl.hpp"

#include "ud_iter.hpp"
#include "ud_msg.hpp"

using namespace rdmaio;

namespace r2
{

const int max_idle_num = 1;

UdAdapter::UdAdapter(const Addr &my_addr, UDQP *sqp, UDQP *qp)
    : my_addr(my_addr), send_qp_(sqp), qp_(qp ? qp : sqp), sender_(my_addr, qp_->local_mem_.key), receiver_(qp_)
{
#if R2_SOLICITED
  if (qp_->event_channel)
  {
    LOG(2) << "R2 msg use event handlers.";
  }
#endif
}

static ibv_ah *
create_ah(UDQP *qp, const QPAttr &attr);

IOStatus
UdAdapter::connect(const Addr &addr, const rdmaio::MacID &id, int uid)
{

  // check if we already connected
  if (connect_infos_.find(addr.to_u32()) != connect_infos_.end())
    return SUCC;

  QPAttr fetched_attr;
  auto ret = QPFactory::fetch_qp_addr(QPFactory::UD, uid, id, fetched_attr);

  if (ret == SUCC)
  {
    auto ah = create_ah(qp_, fetched_attr);
    if (ah == nullptr)
    {
      ASSERT(false);
      return ERR;
    }
    UdConnectInfo connect_info = {.address_handler = ah,
                                  .remote_qpn = fetched_attr.qpn,
                                  .remote_qkey = fetched_attr.qkey};
    connect_infos_.insert(std::make_pair(addr.to_u32(), connect_info));
    return SUCC;
  }
  else
    return ret;
}

IOStatus
UdAdapter::send_async(const Addr &addr, const char *msg, int size)
{

  auto &wr = sender_.cur_wr();
  auto &sge = sender_.cur_sge();

  const auto &it = connect_infos_.find(addr.to_u32());
  if (unlikely(it == connect_infos_.end()))
    return NOT_CONNECT;

  const auto &link_info = it->second;

  wr.wr.ud.ah = link_info.address_handler;
  wr.wr.ud.remote_qpn = link_info.remote_qpn;
  wr.wr.ud.remote_qkey = link_info.remote_qkey;

  wr.send_flags = (send_qp_->empty() ? IBV_SEND_SIGNALED : 0) |
                  ((size < ::rdmaio::MAX_INLINE_SIZE) ? IBV_SEND_INLINE : 0);
#if R2_SOLICITED
  //  if (qp_->event_channel)
  wr.send_flags |= IBV_SEND_SOLICITED;
#endif

  if (send_qp_->need_poll())
  {
    ibv_wc wc;
    auto ret = ::rdmaio::QPUtily::wait_completion(send_qp_->cq_, wc);
    ASSERT(ret == SUCC) << "poll UD completion reply error: " << ret;
    send_qp_->clear();
  }
  else
  {
    send_qp_->forward(1);
  }
  sge.addr = (uintptr_t)msg;
  sge.length = size;
  // FIXME: we assume that the qp_ is appropriate created,
  // so that it's send queue is larger than MAX_UD_SEND_DOORBELL
  sender_.current_window_idx_ += 1;
  if (sender_.current_window_idx_ >=
      std::min(MAX_UD_SEND_DOORBELL, qp_->max_send_size))
    flush_pending();
  return SUCC;
}

IOStatus
UdAdapter::flush_pending()
{
  return sender_.flush_pending(send_qp_);
}

int UdAdapter::poll_all(const MsgProtocol::msg_callback_t &f)
{
#if R2_SOLICITED
  if (qp_->event_channel)
  {
#if 0
    struct pollfd my_pollfd;
    my_pollfd.fd = qp_->event_channel->fd;
    my_pollfd.events = POLLIN;
    my_pollfd.revents = 0;
    auto rc = poll(&my_pollfd, 1, 0);
    ASSERT(rc >= 0);

    if (rc == 0)
      return 0;
#endif
    // There is events, get it
    void *ev_ctx;
    struct ibv_cq *ev_cq;

    // waiting for in-coming requests
    // XD: fixme: this is a blocking call
    if (ibv_get_cq_event(qp_->event_channel, &ev_cq, &ev_ctx))
    {
      ASSERT(false) << "faild to get the cq event";
    }

    if (ibv_req_notify_cq(ev_cq, 1))
    {
      ASSERT(false) << "Couldn't request CQ notification";
    }
  }
#endif
  uint poll_result =
      ibv_poll_cq(qp_->recv_cq_, MAX_UD_RECV_SIZE, receiver_.wcs_);
  for (uint i = 0; i < poll_result; ++i)
  {
    if (likely(receiver_.wcs_[i].status == IBV_WC_SUCCESS))
    {
      Addr addr;
      addr.from_u32(receiver_.wcs_[i].imm_data);
      f((const char *)(receiver_.wcs_[i].wr_id + GRH_SIZE),
        MAX_UD_PACKET_SIZE,
        addr);
    }
    else
    {
      ASSERT(false) << "error wc status " << receiver_.wcs_[i].status;
    }
  }
  if (qp_->event_channel)
  {
    ibv_ack_cq_events(qp_->recv_cq_, 1);
  }
  flush_pending();
  current_idle_recvs_ += poll_result;
  if (current_idle_recvs_ > max_idle_num)
  {
    ASSERT(receiver_.post_recvs(qp_, current_idle_recvs_) == SUCC);
    current_idle_recvs_ = 0;
  }

  return poll_result;
}

Buf_t UdAdapter::get_my_conninfo()
{
  QPAttr res = qp_->get_attr();
  return Marshal::serialize_to_buf(res);
}

IOStatus
UdAdapter::connect_from_incoming(const Addr &addr, const Buf_t &connect_info)
{
  QPAttr attr;
  if (!Marshal::deserialize(connect_info, attr))
    return ERR;
  auto ah = create_ah(qp_, attr);
  if (ah == nullptr)
    return ERR;

  UdConnectInfo info = {.address_handler = ah,
                        .remote_qpn = attr.qpn,
                        .remote_qkey = attr.qkey};
  if (connect_infos_.find(addr.to_u32()) != connect_infos_.end())
    connect_infos_.erase(connect_infos_.find(addr.to_u32()));
  connect_infos_.insert(std::make_pair(addr.to_u32(), info));
  return SUCC;
}

void UdAdapter::disconnect(const Addr &addr)
{
#if 1
  if (connect_infos_.find(addr.to_u32()) != connect_infos_.end())
  {
    auto ah = connect_infos_[addr.to_u32()].address_handler;
    ibv_destroy_ah(ah);
    connect_infos_.erase(connect_infos_.find(addr.to_u32()));
  }
#endif
}

Iter_p_t
UdAdapter::get_iter()
{
  return Iter_p_t(new UDIncomingIter(this));
}

static ibv_ah *
create_ah(UDQP *qp, const QPAttr &attr)
{
  struct ibv_ah_attr ah_attr;
  ah_attr.is_global = 1;
  ah_attr.dlid = attr.lid;
  ah_attr.sl = 0;
  ah_attr.src_path_bits = 0;
  ah_attr.port_num = attr.port_id;

  ah_attr.grh.dgid.global.subnet_prefix = attr.addr.subnet_prefix;
  ah_attr.grh.dgid.global.interface_id = attr.addr.interface_id;
  ah_attr.grh.flow_label = 0;
  ah_attr.grh.hop_limit = 255;
  ah_attr.grh.sgid_index = qp->rnic_p->addr.local_id;
  return ibv_create_ah(qp->rnic_p->pd, &ah_attr);
}

} // end namespace r2
