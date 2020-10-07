#pragma once

#include "rlib/rc.hpp"

#include "../scheduler.hpp"

namespace r2 {

/**
 * RdmaFuture does not track timeout.
 * This is because the ibv_qp track the timeout
 */
class RdmaFuture : public Future<rdmaio::IOStatus>
{
public:
  RdmaFuture(int cor_id, rdmaio::RCQP* qp)
    : Future(cor_id)
    , qp(qp)
  {}

  rdmaio::IOStatus poll(std::vector<int>& routine_count) override
  {
    ibv_wc wc;
    int cor_id;
    if ((cor_id = qp->poll_one_comp(wc)) > 0) {
      ASSERT(routine_count.size() > cor_id);

      //  we decrease the routine counter here
      routine_count[cor_id] -= 1;
      if (routine_count[cor_id] == 0)
        return rdmaio::SUCC;
      return rdmaio::EJECT;
    } else if (cor_id == 0)
      return rdmaio::NOT_READY;
    else {
      // TODO: fix error cases
      return rdmaio::ERR;
    }
  }

  static void spawn_future(RScheduler& s, rdmaio::RCQP* qp, int num = 1,int test_id =  0)
  {
    using namespace rdmaio;
    auto id = s.cur_id();
    if (likely(num > 0))
      s.emplace(
        s.cur_id(),
        num,
        [test_id,id, qp](std::vector<int>& routine_count) -> RScheduler::poll_result_t {
          int cor_id;
          if (routine_count[id] == 0) {
            return std::make_pair(SUCC, id);
          }

          ibv_wc wc;
          if ((cor_id = qp->poll_one_comp(wc)) &&
              (wc.status == IBV_WC_SUCCESS)) {
            ASSERT(routine_count.size() > cor_id);
            ASSERT(routine_count[cor_id] >= 1)
              << "polled an invalid cor_id: " << cor_id;
            //  we decrease the routine counter here
            routine_count[cor_id] -= 1;
          }
          if (unlikely(cor_id != 0 && (wc.status != IBV_WC_SUCCESS))) {
            LOG(4) << "poll till completion error: " << wc.status << " "
                   << ibv_wc_status_str(wc.status) << " at scheduler with id: ";
            // TODO： we need to filter out timeout events
            return std::make_pair(NOT_READY,
                                  cor_id); // this SUCC only indicates the
                                           // scheduler to eject this coroutine
          }
          if (routine_count[id] == 0) {
            return std::make_pair(SUCC, id);
          }

          return std::make_pair(NOT_READY, 0);
        });
  }

  static rdmaio::IOStatus send_wrapper(RScheduler& s,
                                       rdmaio::RCQP* qp,
                                       int cor_id,
                                       const rdmaio::RCQP::ReqMeta& meta,
                                       const rdmaio::RCQP::ReqContent& req)
  {
    auto res = qp->send(meta, req);

    if (likely(res == rdmaio::SUCC))
      spawn_future(s, qp, 1);
    return res;
  }

private:
  rdmaio::RCQP* qp;
}; // namespace r2

} // end namespace r2
