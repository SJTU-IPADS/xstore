#pragma once

#include "../timer.hpp"
#include "rlib/common.hpp"

#include "future.hpp"

namespace r2 {

/**
 * The RPC future is very simple, because it just check
 * whether the reply of this RPC is timedout.
 * So if the routine_count (pending futures for this routine) is 0,
 * then it should return success.
 */
class RpcFuture : public Future<rdmaio::IOStatus> {
 public:
  RpcFuture(int cor_id,double timeout = 1000) :
      Future(cor_id),
      timeout(timeout) {
  }

  rdmaio::IOStatus poll(std::vector<int> &routine_count) override {
    if(routine_count[cor_id] == 0)
      return rdmaio::SUCC;
    if(timer_.passed_msec() >= timeout) {
      return rdmaio::TIMEOUT;
    } else
      return rdmaio::NOT_READY;
  }

 private:
  Timer timer_;
  double timeout;
}; // end class

} // end namespace r2
