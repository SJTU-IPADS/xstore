#pragma once

#include <vector>

#include "../../../deps/r2/src/mem_block.hh"

/*!
  Data structure for recording the replies
 */

namespace xstore {

namespace rpc {

using namespace r2;

struct ReplyEntry {
  usize pending_replies = 0;
  MemBlock reply_buf;
};

/*!
  Record the reply entries of all coroutines.
  Currently, we assume fixed coroutines stored using vector.
  The number of coroutines must be passed priori to the creation of the station.
 */
struct ReplyStation {
  // TODO: reply vector with T, which implements a mapping from corid ->
  // ReplyEntry
  std::vector<ReplyEntry> cor_replies;

  explicit ReplyStation(int num_cors) : cor_replies(num_cors) {}

  /*!
    return whether there are pending replies
   */
  auto add_pending_reply(int cor_id, const ReplyEntry &reply) -> bool {
    ASSERT(cor_id < cor_replies.size());
    if (cor_replies[cor_id].pending_replies > 0) {
      return false;
    }
    cor_replies[cor_id] = reply;
    return true;
  }
};
} // namespace rpc
} // namespace xstore
