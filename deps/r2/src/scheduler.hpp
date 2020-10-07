#pragma once

#include "futures/future.hpp"
#include "timeout_manager.hpp"
#include "rexecutor.hpp"

#include "rlib/common.hpp"

#include <deque>
#include <vector>

/*!
  RScheduler is the executor who execute R2 routines.
  An example usage is:
  `
    RScheduler s;
    s.spawnr([](R2_ASYNC) {
      R2_RET;
    });
    s.run();
  `
  This spawns a corutine which does nothing.

  In each coroutine, applications can execute multiple R2 calls.
  For example:
  `
  R2_RET; // coroutine should return with R2_RET key world. Otherwise, the behavior is undefined.
  R2_YIELD; // yield this coroutine to another;
  `
  For more keyword, please refer to scheduler_macros.hpp.
*/
namespace r2
{

class RScheduler : public RExecutor
{
public:
  using poll_result_t = std::pair<rdmaio::IOStatus, int>;
  using poll_func_t = std::function<poll_result_t(std::vector<int> &)>;
  using Futures = std::deque<poll_func_t>;
  using routine_t = std::function<void(handler_t &yield, RScheduler &r)>;
  // TODO, XD:
  // Shall we leave the futures per coroutine, instead poll all futures per
  // schedule?
  //  const usize id = 0;

private:
  Futures poll_futures_;
  TM tm;
  bool running_ = true;

public:
  RScheduler();

  //RScheduler(usize id) : id(id) { }

  explicit RScheduler(const routine_t &f);

  int spawnr(const routine_t &f);

  void emplace(Future<rdmaio::IOStatus> &f)
  {
    ASSERT(false) << " legacy API, should not be used any more.";
  }

  void emplace(int corid, int num, poll_func_t f)
  {
    pending_futures_[corid] += num;
    poll_futures_.push_back(f);
  }

  void wait_for(const u64 &time)
  {
    tm.enqueue(cur_id(), read_tsc() + time, cur_routine_->seq);
  }

  IOStatus wait_for(const u64 &time, handler_t &yield)
  {
    if (pending_futures_[cur_id()] > 0)
    {
      wait_for(time);
      return pause_and_yield(yield);
    }
    return SUCC;
  }

  IOStatus pause(handler_t &yield)
  {
    if (pending_futures_[cur_id()] > 0)
      return pause_and_yield(yield);
    return SUCC;
  }

  /**
   * Stop & resume scheduling coroutines
   */
  void stop_schedule();

  /*!
   * Poll all the registered futures.
   */
  void poll_all();

  bool is_running() const { return running_; }

  std::vector<int> pending_futures_;

  DISABLE_COPY_AND_ASSIGN(RScheduler);
}; // end class

} // namespace r2

#include "scheduler_marocs.hpp"
