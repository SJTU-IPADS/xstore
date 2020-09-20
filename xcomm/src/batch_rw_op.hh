#pragma once

#include "./lib.hh"

#include "../../deps/rlib/core/qps/op.hh"
#include "../../deps/r2/src/rdma/async_op.hh"

namespace xstore {

namespace xcomm {

using namespace rdmaio;
using namespace qp;
using namespace r2;

template <usize N> struct BatchOp {
  ::rdmaio::qp::Op<1> ops[N];
  int cur_idx;

  BatchOp() : ops(), cur_idx(-1) {
    for (uint i = 0; i < N; ++i) {
      ops[i].set_next(&ops[i + 1]);
    }
  }

  auto emplace() { cur_idx += 1; }

  auto get_cur_op() -> ::rdmaio::qp::Op<1> & { return ops[cur_idx]; }

  auto reset() { cur_idx = -1; }

  /*!
    \note: Assumption!!!!
    we assumes all ops are unsignaled.
    We will manually signal the last op.
   */
  auto execute_async(const Arc<RC> &qp, R2_ASYNC) -> Result<ibv_wc> {
    ::r2::rdma::AsyncOp<> dummy;
    RC *qp_ptr = ({ // unsafe code
      RC *temp = qp.get();
      temp;
    });

    ibv_wc wc;
    if (unlikely(cur_idx < 0)) {
      // trivial
      return ::rdmaio::Ok(wc);
    }

    // sanity check
    ASSERT(cur_idx < N);

    // set the flag
    auto &op = this->get_cur_op();
    int temp_flag = op.wr.send_flags;
    op.set_flags(temp_flag | IBV_SEND_SIGNALED);
    op.wr.next = nullptr;
    op.wr.wr_id = qp_ptr->encode_my_wr(R2_COR_ID(),cur_idx + 1);

    // 2. send the doorbell
    struct ibv_send_wr *bad_sr = nullptr;
    auto rc = ibv_post_send(qp_ptr->qp, &(this->ops[0].wr), &bad_sr);
    if (unlikely(rc != 0)) {
      return ::rdmaio::Err(wc);
    }
    qp_ptr->out_signaled += 1;

    auto ret = dummy.wait_one(qp, R2_ASYNC_WAIT);

    // reset
    op.set_next(&ops[cur_idx + 1]);
    op.wr.send_flags = temp_flag;

    // should be invalid
    this->reset();

    return ret;
  }
};

} // namespace xcomm
} // namespace xstore
