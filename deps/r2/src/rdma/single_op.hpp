#pragma once

#include "../scheduler.hpp"
#include "rlib/rc.hpp"

namespace r2
{
namespace rdma
{
using namespace rdmaio;

/*!
 SROP states for single RDMA one-sided OP.
 example usage:  // read 3 bytes at remote machine with address 0xc using
 one-sided RDMA.
      ::r2::rdma::SROp op(qp);
      op.set_payload(ptr,3).set_remote_addr(0xc).set_op(IBV_WR_RDMA_READ);
      auto ret = op.execute(IBV_SEND_SIGNALED,R2_ASYNC_WAIT); // async version
      ASSERT(std::get<0>(ret) == SUCC);

      or
      auto ret = op.execute_sync(); // sync version
 */
class SROp
{
public:
  RCQP *qp = nullptr;
  ibv_wr_opcode op;

  u64 remote_addr;

  char *local_ptr = nullptr;
  usize size = 0;
  int flags = 0;

public:
  explicit SROp(RCQP *qp) : qp(qp) {}

  inline SROp &set_payload(char *ptr, usize size)
  {
    local_ptr = ptr;
    this->size = size;
    return *this;
  }

  inline SROp &set_op(const ibv_wr_opcode &op)
  {
    this->op = op;
    return *this;
  }

  inline SROp &set_read()
  {
    this->op = IBV_WR_RDMA_READ;
    return *this;
  }

  inline SROp &set_write()
  {
    this->op = IBV_WR_RDMA_WRITE;
    return *this;
  }

  inline SROp &set_remote_addr(const u64 &ra)
  {
    remote_addr = ra;
    return *this;
  }

  inline auto execute(R2_ASYNC)
      -> std::tuple<IOStatus, struct ibv_wc>
  {
    return execute(IBV_SEND_SIGNALED, R2_ASYNC_WAIT);
  }

  /*!
   TODO: add timeout
   */
  inline auto execute_sync()
      -> std::tuple<IOStatus, struct ibv_wc>
  {
    ibv_wc wc;
    auto res = qp->send(
        {.op = op,
         .flags = flags | IBV_SEND_SIGNALED,
         .len = size,
         .wr_id = 0 /*a dummy record*/},
        {.local_buf = local_ptr, .remote_addr = remote_addr, .imm_data = 0});
    if (res == SUCC)
      res = qp->wait_completion(wc);
    return std::make_pair(res, wc);
  }

  inline auto execute(int flags, R2_ASYNC)
      -> std::tuple<IOStatus, struct ibv_wc>
  {
    ibv_wc wc;
    auto res = qp->send(
        {.op = op, .flags = flags, .len = size, .wr_id = R2_COR_ID()},
        {.local_buf = local_ptr, .remote_addr = remote_addr, .imm_data = 0});
    if (unlikely(res != SUCC))
      return std::make_pair(res, wc);
    // spawn a future
    if (flags & IBV_SEND_SIGNALED)
    {
      auto id = R2_COR_ID();
      auto poll_future =
          [this, id, &res,
           &wc](std::vector<int> &routine_count) -> RScheduler::poll_result_t {
        if (routine_count[id] == 0)
        {
          res = SUCC;
          return std::make_pair(SUCC, id);
        }

        int cor_id;
        if ((cor_id = qp->poll_one_comp(wc)) && (wc.status == IBV_WC_SUCCESS))
        {
          ASSERT(routine_count.size() > cor_id);
          ASSERT(routine_count[cor_id] >= 1)
              << "polled an invalid cor_id: " << cor_id;
          //  we decrease the routine counter here
          routine_count[cor_id] -= 1;
        }
        else
        {
          if (unlikely(cor_id != 0))
          {
            LOG(4) << "poll till completion error: " << wc.status << " "
                   << ibv_wc_status_str(wc.status);
            // TODO： we need to filter out timeout events
            return std::make_pair(SUCC,
                                  cor_id); // this SUCC only indicates the
                                           // scheduler to eject this coroutine
          }
        }
        if (routine_count[id] == 0)
        {
          res = SUCC;
          return std::make_pair(SUCC, id);
        }
        return std::make_pair(NOT_READY, 0);
      };
      // if the request is signaled, we emplace a poll future
      res = R2_PAUSE_WAIT(poll_future, 1);
      // the results will be encoded in wc, so its fine
      /*
        Actually we can use YIELD, to avoid a future.
        However, since calling between coroutines are much costly than calling a
        function, so we remove the coroutine from this list
       */
    }

    return std::make_pair(res, wc);
  }
};
} // namespace rdma

} // namespace r2
