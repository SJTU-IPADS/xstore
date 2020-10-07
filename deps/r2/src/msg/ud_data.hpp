#pragma once

#include "../allocator_master.hpp"
#include "../logging.hpp"

namespace r2 {

const int MAX_UD_SEND_DOORBELL = 16;
const int MAX_UD_RECV_SIZE = 2048;
const int MAX_UD_PACKET_SIZE = 4096;
const int GRH_SIZE = 40;

struct UdConnectInfo
{
  ibv_ah* address_handler = nullptr;
  u64 remote_qpn = 0;
  u64 remote_qkey = 0;
};

struct UdSender
{

  UdSender(const Addr& addr, uint64_t lkey)
  {

    for (uint i = 0; i < MAX_UD_SEND_DOORBELL; ++i) {
      wrs_[i].opcode = IBV_WR_SEND_WITH_IMM;
      wrs_[i].num_sge = 1;
      wrs_[i].imm_data = addr.to_u32();

      wrs_[i].next = &wrs_[i + 1];
      wrs_[i].sg_list = &sges_[i];
      sges_[i].lkey = lkey;
    }
  }

  void reset_key(const u64 &lkey) {
    for (uint i = 0; i < MAX_UD_SEND_DOORBELL; ++i)  {
      sges_[i].lkey = lkey;
    }
  }

  inline rdmaio::IOStatus flush_pending(rdmaio::UDQP* qp)
  {
    if (current_window_idx_ > 0) {
      wrs_[current_window_idx_ - 1].next = nullptr;
      struct ibv_send_wr* bad_sr_ = nullptr;
      auto ret = ibv_post_send(qp->qp_, &wrs_[0], &bad_sr_);
      ASSERT(ret == 0) << "; w error: " << strerror(errno);
      wrs_[current_window_idx_ - 1].next = &wrs_[current_window_idx_];
      current_window_idx_ = 0; // clear
    }
    return rdmaio::SUCC;
  }

  ibv_send_wr& cur_wr() { return wrs_[current_window_idx_]; }
  ibv_sge& cur_sge() { return sges_[current_window_idx_]; }

  ibv_send_wr wrs_[MAX_UD_SEND_DOORBELL];
  ibv_sge sges_[MAX_UD_SEND_DOORBELL];

  uint current_window_idx_ = 0;
};

struct UdReceiver
{

  UdReceiver(rdmaio::UDQP* qp, int max_msg_size = MAX_UD_PACKET_SIZE)
  {

    ASSERT(qp != nullptr);
    auto allocator = AllocatorMaster<>::get_thread_allocator();
    ASSERT(allocator != nullptr);

    if (max_msg_size > MAX_UD_PACKET_SIZE)
      max_msg_size = MAX_UD_PACKET_SIZE - GRH_SIZE;

    // fill in the default values of recv wrs/sges
    for (uint i = 0; i < MAX_UD_RECV_SIZE; ++i) {
      struct ibv_sge sge
      {
        .addr = (uintptr_t)allocator->alloc(max_msg_size),
        .length = (uint32_t)max_msg_size, .lkey = qp->local_mem_.key
      };
      ASSERT(sge.addr != 0) << "failed to allocate recv buffer.";
      sges_[i] = sge;

      rrs_[i].wr_id = sges_[i].addr;
      rrs_[i].sg_list = &sges_[i];
      rrs_[i].num_sge = 1;

      rrs_[i].next = (i < (MAX_UD_RECV_SIZE - 1)) ? &rrs_[i + 1] : &rrs_[0];
    }
#if R2_SOLICITED
    if (qp->event_channel && ibv_req_notify_cq(qp->recv_cq_, 1)) {
      ASSERT(false) << "Couldn't request CQ notification";
    }

#if 0
    // making the event non-blocking
    auto flags = fcntl(qp->event_channel->fd, F_GETFL);
    auto rc = fcntl(qp->event_channel->fd, F_SETFL, flags | O_NONBLOCK);
    if (rc < 0) {
      ASSERT(false)
          << "Failed to change file descriptor of Completion Event Channel";
    }
#endif
#endif
    post_recvs(qp, MAX_UD_RECV_SIZE);
  }

  struct ibv_recv_wr rrs_[MAX_UD_RECV_SIZE];
  struct ibv_sge sges_[MAX_UD_RECV_SIZE];
  struct ibv_wc wcs_[MAX_UD_RECV_SIZE];
  struct ibv_recv_wr* bad_rr_;

  // current header which pointes to the rrs
  int recv_head_ = 0;

  rdmaio::IOStatus post_recvs(rdmaio::UDQP* qp, int num)
  {
    if (unlikely(num <= 0))
      return rdmaio::SUCC;

    auto tail = recv_head_ + num - 1;
    if (tail >= MAX_UD_RECV_SIZE)
      tail -= MAX_UD_RECV_SIZE;

    // record the current next pointer, and set tailer to null
    auto temp = std::exchange((rrs_ + tail)->next, nullptr);

    auto rc = ibv_post_recv(qp->qp_, rrs_ + recv_head_, &bad_rr_);
    if (rc != 0) {
      LOG(4) << "post recv " << num << "; w error: " << strerror(errno);
      return rdmaio::ERR;
    }
    (rrs_ + tail)->next = temp;
    recv_head_ = (tail + 1) % MAX_UD_RECV_SIZE;

    return rdmaio::SUCC;
  }
};

} // end namespace r2
