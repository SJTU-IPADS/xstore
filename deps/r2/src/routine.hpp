#pragma once

#include "logging.hpp"
#include "common.hpp"
#include "rlib/common.hpp"

#include <vector>
#include <functional>

#include <boost/coroutine/all.hpp>

namespace r2
{

typedef boost::coroutines::symmetric_coroutine<void>::yield_type handler_t;
typedef boost::coroutines::symmetric_coroutine<void>::call_type coroutine_func_t;
typedef std::function<void(handler_t &yield)> internal_routine_t;
using namespace rdmaio;

class RExecutor;

class RoutineLink
{
  friend class RExecutor;

  class Routine
  {
  public:
    // the executing function
    coroutine_func_t func_;
    Routine *prev_routine_ = nullptr;
    Routine *next_routine_ = nullptr;
    bool active_ = false;
    IOStatus status = SUCC;
    // my id
    const u8 id_;
    u32 seq = 0;

    // a wrapper over status
    std::shared_ptr<internal_routine_t> unwrapperd_fuc_;

  public:
    Routine(u8 id, std::shared_ptr<internal_routine_t> &f)
        : id_(id), func_(*f),
          unwrapperd_fuc_(f)
    {
    }

    inline Routine *leave(RoutineLink &c)
    {
      active_ = false;
      auto next = next_routine_;
      ASSERT(prev_routine_ != nullptr);
      prev_routine_->next_routine_ = next;
      next_routine_->prev_routine_ = prev_routine_;
      if (c.tailer_ == this)
        c.tailer_ = prev_routine_;
      return next_routine_;
    }

    /**
     * execute this the routine callable function
     */
    inline void execute(handler_t &yield)
    {
      active_ = true;
      yield(func_);
    }
  }; // end class Routine

  friend class Routine;
  Routine *header_ = nullptr;
  Routine *tailer_ = nullptr;

public:
  RoutineLink() = default;
  ~RoutineLink() = default;

  inline Routine *append(Routine *r)
  {
    auto prev = tailer_;
    if (unlikely(header_ == nullptr))
    {
      // this routine is the header
      tailer_ = (header_ = r);
      prev = tailer_;
    }
    ASSERT(tailer_ != nullptr);
    tailer_->next_routine_ = r;

    // this routine is the new tailer
    tailer_ = r;

    // my position
    r->prev_routine_ = prev;
    r->next_routine_ = header_;
    return r;
  }

  DISABLE_COPY_AND_ASSIGN(RoutineLink);
};

} // namespace r2
