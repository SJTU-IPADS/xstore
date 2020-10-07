#pragma once

#include "../deps/rlib/rc.hpp"

namespace fstore {

namespace X {

using namespace rdmaio;

// fetch at most MAX pages in a doorbelled way
template<int MAX>
class RemoteFetcher
{
public:
  RemoteFetcher(RCQP* qp, int corid)
    : qp(qp)
    , cor_id(corid)
  {}

  int add(u64 page_addr, u64 size, char* local_buf)
  {
    auto idx = pending++;
    auto& sge = sges[idx];
    sge.length = size;
    sge.lkey = qp->local_mem_.value().key;
    sge.addr = (u64)local_buf;

    init_one(idx);
    auto& sr = srs[idx];
    sr.wr.rdma.remote_addr = page_addr + qp->remote_mem_.value().buf;
    return pending;
  }

  /*!
    Flush all pending RDMA reqs
   */
  IOStatus flush(int idx)
  {
    auto& sr = srs[idx];
    sr.next = nullptr;
    sr.send_flags |= IBV_SEND_SIGNALED;
    auto ret = qp->send(&srs[0], &bad_sr);
    pending = 0;
    return ret;
  }

public:
  struct ibv_send_wr srs[MAX];
  struct ibv_sge sges[MAX];
  struct ibv_send_wr* bad_sr;

private:
  RCQP* qp = nullptr;
  const int cor_id = -1;
  int pending = 0;

  inline void init_one(int idx)
  {
    auto& sr = srs[idx];
    sr.wr_id = (static_cast<u64>(cor_id) << Progress::num_progress_bits) |
               qp->progress_.forward(1);
    sr.opcode = IBV_WR_RDMA_READ;
    sr.num_sge = 1;
    sr.sg_list = &(sges[idx]);
    sr.send_flags = 0;
    sr.next = &(srs[idx + 1]);
    sr.wr.rdma.rkey = qp->remote_mem_.value().key;
  }
};

}

} // fstore
