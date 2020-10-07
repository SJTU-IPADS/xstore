#pragma once

//#include "../scheduler.hpp"

#include "msg.hpp"
#include "net_naming.hpp"
#include "scheduler.hpp"

#include "rlib/common.hpp"

namespace r2 {
/*!
    Session manages a connection from myself to a specific remote point.
*/
class Session {
public:
  static constexpr double no_timeout = std::numeric_limits<double>::max();
  /*!
      Send a message, and spawn a future in the scheduler

      \param msg: the sending msg
      \param timeout: time out recorded in **ms**

      usage example:
          auto future = session.send(msg,120);
          R2_SPAWN(future);
          R2_YIELD;
   */
  /*!
    res, Option<The detailed reason of err>
   */
  using msg_result_t = std::pair<rdmaio::IOStatus, std::string>;
  virtual msg_result_t send(const Msg &msg, const double timeout, R2_ASYNC) = 0;

  /*!
      A blocking version of send.

      \param msg: the sending msg
      \param timeout: time out recorded in **ms**

      usage example:
          auto ret = session.send_blocking(msg,120); // 120 ms
          ASSERT(ret == SUCC);
   */
  virtual msg_result_t send_blocking(const Msg &msg,
                                     const double timeout = no_timeout) = 0;

  /*!
      XD:Should recording pending (un-sent message)
      Post a request to the RNIC, and ignore its completion.
   */
  virtual rdmaio::IOStatus send_pending(const Msg &msg) = 0;

  /*!
      Async, coroutine version
   */
  virtual rdmaio::IOStatus send_pending(const Msg &msg, R2_ASYNC) = 0;
};

} // namespace r2
