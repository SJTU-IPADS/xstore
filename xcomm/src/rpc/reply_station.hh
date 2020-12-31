#pragma once

#include <vector>

#include "../../../deps/r2/src/mem_block.hh"

/*!
  Data structure for recording the replies
 */

namespace xstore {

namespace rpc {

using namespace r2;

struct ReplyEntry
{
  usize pending_replies = 0;
  MemBlock reply_buf;
  char* cur_ptr;

  ReplyEntry(const MemBlock& reply_buf)
    : pending_replies(1)
    , reply_buf(reply_buf)
    , cur_ptr(reinterpret_cast<char*>(reply_buf.mem_ptr))
  {}

  ReplyEntry() = default;
};

/*!
  Record the reply entries of all coroutines.
  Currently, we assume fixed coroutines stored using vector.
  The number of coroutines must be passed priori to the creation of the station.
 */
struct ReplyStation
{
  // TODO: reply vector with T, which implements a mapping from corid ->
  // ReplyEntry
  std::vector<ReplyEntry> cor_replies;

  explicit ReplyStation(int num_cors)
    : cor_replies(num_cors)
  {}

  /*!
    return whether there are pending replies
   */
  auto add_pending_reply(const int& cor_id, const ReplyEntry& reply) -> bool
  {
    ASSERT(cor_id < cor_replies.size());
    if (cor_replies[cor_id].pending_replies > 0) {
      return false;
    }
    cor_replies[cor_id] = reply;
    return true;
  }

  auto cor_ready(const int& cor_id) -> bool
  {
    return cor_replies[cor_id].pending_replies == 0;
  }

  /*!
    \ret: whether append reply is ok
   */
  auto append_reply(const int& cor_id, const MemBlock& payload) -> bool
  {
    if (unlikely(this->cor_ready(cor_id))) {
      return false;
    }

    auto& r = this->cor_replies[cor_id];
    ASSERT(r.cur_ptr + payload.sz <=
           (char*)r.reply_buf.mem_ptr + r.reply_buf.sz)
      << "overflow reply sz: " << payload.sz
      << "; total reply: " << r.reply_buf.sz;
    memcpy(r.cur_ptr, payload.mem_ptr, payload.sz);
    r.cur_ptr += payload.sz;

    r.pending_replies -= 1;
    return true;
  }
};
} // namespace rpc
} // namespace xstore
