#pragma once

#include "qp_config.hpp"
#include "rnic.hpp"

namespace rdmaio
{

class Info
{
public:
  static std::string qp_addr_to_str(const qp_address_t &addr)
  {
    std::stringstream ostr("{");
    ostr << "subnet_prefix: " << addr.subnet_prefix << ";"
         << "interface_id:  " << addr.interface_id << ";"
         << "local_id:      " << addr.local_id << "}";
    return ostr.str();
  }
};

// some utilities function
inline int convert_mtu(ibv_mtu type)
{
  int mtu = 0;
  switch (type)
  {
  case IBV_MTU_256:
    mtu = 256;
    break;
  case IBV_MTU_512:
    mtu = 512;
    break;
  case IBV_MTU_1024:
    mtu = 1024;
    break;
  case IBV_MTU_2048:
    mtu = 2048;
    break;
  case IBV_MTU_4096:
    mtu = 4096;
    break;
  }
  return mtu;
}

inline Duration_t us_to_duration(const double &us)
{
  return {0, static_cast<__suseconds_t>(us)};
}

// return t1 - t0
inline u64 time_gap(const Duration_t &t1, const Duration_t &t0)
{
  if (t1.tv_sec < t1.tv_sec)
    return 0;
  return ((t1.tv_sec - t0.tv_sec) * 1000000L + (t1.tv_usec - t0.tv_usec));
}

inline u64 time_to_micro(const Duration_t &t)
{
  return t.tv_sec * 1000000L + t.tv_usec;
}

class QPUtily
{
public:
  static ibv_cq *create_cq(const RNic &rnic, int size,
                           struct ibv_comp_channel *channel = nullptr,
                           void *ev_ctx = nullptr)
  {
    return ibv_create_cq(rnic.ctx, size, ev_ctx, channel, 0);
  }

  static ibv_qp *create_qp(ibv_qp_type type, const RNic &rnic,
                           const QPConfig &config, ibv_cq *send_cq,
                           ibv_cq *recv_cq)
  {
    struct ibv_qp_init_attr qp_init_attr = {};

    qp_init_attr.send_cq = send_cq;
    qp_init_attr.recv_cq = recv_cq;
    qp_init_attr.qp_type = type;

    qp_init_attr.cap.max_send_wr = config.max_send_size;
    qp_init_attr.cap.max_recv_wr = config.max_recv_size;
    qp_init_attr.cap.max_send_sge = 1;
    qp_init_attr.cap.max_recv_sge = 1;
    qp_init_attr.cap.max_inline_data = MAX_INLINE_SIZE;

    auto qp = ibv_create_qp(rnic.pd, &qp_init_attr);
    if (qp != nullptr)
    {
      auto res = bring_qp_to_init(qp, rnic, config);
      if (res)
        return qp;
    }
    return nullptr;
  }

  static bool destroy_qp(ibv_qp *qp)
  {
    int rc = ibv_destroy_qp(qp);
    RDMA_VERIFY(WARNING, rc == 0) << "Failed to destroy QP " << strerror(errno);

    return rc == 0;
  }

  static IOStatus wait_completion(ibv_cq *cq, ibv_wc &wc,
                                  const Duration_t &timeout = no_timeout)
  {

    Duration_t start;
    gettimeofday(&start, nullptr);
    Duration_t now;
    gettimeofday(&now, nullptr);
    int poll_result(0);

    do
    {
      asm volatile("" ::
                       : "memory");
      poll_result = ibv_poll_cq(cq, 1, &wc);
      gettimeofday(&now, nullptr);
    } while (poll_result == 0 &&
             !(time_gap(now, start) < time_to_micro(timeout)));

    if (poll_result == 0)
      return TIMEOUT;
    if (poll_result < 0 || wc.status != IBV_WC_SUCCESS)
    {
      RDMA_LOG_IF(4, wc.status != IBV_WC_SUCCESS)
          << "poll till completion error: " << wc.status << " "
          << ibv_wc_status_str(wc.status);
      return ERR;
    }
    return SUCC;
  }

  static bool bring_qp_to_init(ibv_qp *qp, const RNic &rnic,
                               const QPConfig &config)
  {

    if (qp != nullptr)
    {
      struct ibv_qp_attr qp_attr = {};
      qp_attr.qp_state = IBV_QPS_INIT;
      qp_attr.pkey_index = 0;
      qp_attr.port_num = rnic.id.port_id;

      int flags = IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT;
      if (qp->qp_type == IBV_QPT_RC)
      {
        qp_attr.qp_access_flags = config.access_flags;
        flags |= IBV_QP_ACCESS_FLAGS;
      }

      if (qp->qp_type == IBV_QPT_UD)
      {
        qp_attr.qkey = config.qkey;
        flags |= IBV_QP_QKEY;
      }

      int rc = ibv_modify_qp(qp, &qp_attr, flags);
      RDMA_VERIFY(WARNING, rc == 0)
          << "Failed to modify QP to INIT state " << strerror(errno)
          << "; use port id: " << rnic.id.port_id;

      if (rc != 0)
      {
        RDMA_LOG(WARNING) << " change state to init failed. ";
        return false;
      }
      return true;
    }
    return false;
  }
}; // end class QPUtily

} // end namespace rdmaio
