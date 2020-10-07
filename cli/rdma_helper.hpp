#pragma once

#include "rlib/rdma_ctrl.hpp"
#include "server/internal/table.hpp"

namespace fstore {

/*!
  Fetch one key and value from a specific page
 */
class PagePointFetcher {
 public:
  PagePointFetcher(RCQP *qp, int cor_id,u64 page_addr, u64 offset,char *local_buf) {
    sge[0].addr = (u64)(local_buf);
    sge[1].addr = (u64)(local_buf + sizeof(u64));

    sge[0].length = (sge[1].length = sizeof(u64));
    sge[0].lkey   = (sge[1].lkey = qp->local_mem_.key);

    sr[1].wr_id = (static_cast<u64>(cor_id) << 32) | qp->progress_.forward(2);
    //sr[0].wr_id = (static_cast<u64>(cor_id) << 32) | qp->progress_.forward(1);
    sr[0].opcode = (sr[1].opcode = IBV_WR_RDMA_READ);
    sr[0].num_sge = (sr[1].num_sge = 1);
    sr[0].sg_list = &(sge[0]);
    sr[1].sg_list = &(sge[1]);
    sr[0].send_flags = 0;
    //sr[0].send_flags = IBV_SEND_SIGNALED;
    sr[1].send_flags = IBV_SEND_SIGNALED;
    sr[0].next = &(sr[1]);
    //sr[0].next = nullptr;
    sr[1].next = nullptr;

    sr[0].wr.rdma.rkey = (sr[1].wr.rdma.rkey = qp->remote_mem_.key);
    sr[0].wr.rdma.remote_addr = page_addr + offsetof(Leaf, keys[offset]) + qp->remote_mem_.buf;
    sr[1].wr.rdma.remote_addr = page_addr + offsetof(Leaf, values[offset]) + qp->remote_mem_.buf;
  }

 public:
  struct ibv_send_wr sr[2];
  struct ibv_sge     sge[2];
  struct ibv_send_wr *bad_sr;
};

} // fstore
