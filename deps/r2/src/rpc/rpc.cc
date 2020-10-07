#include "rpc.hpp"
#include "../common.hpp"
#include "rpc_handlers.hpp"

using namespace rdmaio;

namespace r2
{

namespace rpc
{

static_assert(END_HS < RESERVED_RPC_ID,
              "The RPC internal uses too many RPC ids!");

static const int kMaxRPCFuncSupport = 16;

RPC::RPC(std::shared_ptr<MsgProtocol> msg_handler)
    : msg_handler_(msg_handler), buf_factory_(padding_ + sizeof(Req::Header))
{
  rpc_callbacks_.resize(kMaxRPCFuncSupport);
  replies_.resize(RExecutor::kMaxCoroutineSupported);

  /**
   * Register our default RPC handler
   */
  register_callback(START_HS, RPCHandler::start_handshake_handler);
  register_callback(END_HS, RPCHandler::stop_handshake_handler);
}

void RPC::register_callback(int rpc_id, rpc_func_t callback)
{
  ASSERT(rpc_id >= 0 && rpc_id < kMaxRPCFuncSupport);
  rpc_callbacks_[rpc_id] = callback;
}

static inline Req::Header
make_rpc_header(int type, int id, int size, int cid)
{
  return {.type = static_cast<u32>(type),
          .rpc_id = static_cast<u32>(id),
          .payload = static_cast<u32>(size),
          .cor_id = static_cast<u32>(cid)};
}

IOStatus
RPC::call(const Req::Meta &context, int rpc_id, const Req::Arg &arg)
{
  auto ret = call_async(context, rpc_id, arg);
  if (likely(ret == SUCC))
    ret = flush_pending();
  return ret;
}

IOStatus
RPC::call_async(const Req::Meta &context, int rpc_id, const Req::Arg &arg)
{

  replies_[context.cor_id].reply_buf = arg.reply_buf;
  replies_[context.cor_id].reply_count += arg.reply_cnt;

  Req::Header *header = (Req::Header *)(arg.send_buf - sizeof(Req::Header));
  *header = make_rpc_header(REQ, rpc_id, arg.len, context.cor_id);
  return msg_handler_->send_async(
      context.dest, (char *)header, sizeof(Req::Header) + arg.len);
}

IOStatus
RPC::reply(const Req::Meta &context, char *reply, int size)
{
  auto ret = reply_async(context, reply, size);
  if (likely(ret == SUCC))
    ret = flush_pending();
  return ret;
}

IOStatus
RPC::reply_async(const Req::Meta &context, char *reply, int size)
{
  Req::Header *header = (Req::Header *)(reply - sizeof(Req::Header));
  *header = make_rpc_header(REPLY, 0 /* a place holder*/, size, context.cor_id);
  return msg_handler_->send_async(
      context.dest, (char *)header, sizeof(Req::Header) + size);
}

inline bool
RPC::sanity_check_reply(const Req::Header *header)
{
  //ASSERT(header->cor_id < replies_.size());
  assert(replies_[header->cor_id].reply_count > 0);
  return replies_[header->cor_id].reply_count > 0;
}

rdmaio::IOStatus
RPC::start_handshake(const Addr &dest, RScheduler &s, handler_t &h)
{
  auto info = msg_handler_->get_my_conninfo();
  char *msg_buf = get_buf_factory().alloc(info.size());
  memcpy(msg_buf, info.data(), info.size());
  auto ret = call({.cor_id = s.cur_id(), .dest = dest},
                  START_HS,
                  {.send_buf = msg_buf,
                   .len = info.size(),
                   .reply_buf = nullptr,
                   .reply_cnt = 1});
  get_buf_factory().dealloc(msg_buf);
  if (ret != SUCC)
  {
    return ret;
  }
  ret = s.pause_and_yield(h);

  return ret;
}

rdmaio::IOStatus
RPC::end_handshake(const Addr &dest)
{
  char *msg_buf = get_buf_factory().alloc(0);
  auto ret = call(
      {.cor_id = 0, .dest = dest},
      END_HS,
      {.send_buf = msg_buf, .len = 0, .reply_buf = nullptr, .reply_cnt = 0});
  get_buf_factory().dealloc(msg_buf);
}

/**
 * spawn a future to handle in-coming req/replies
 */
void RPC::poll_all(RScheduler &s, std::vector<int> &routine_count)
{
  auto temp = read_tsc();

  usize num_msgs = 0;
  msg_handler_->poll_all([&](const char *msg, int size, const Addr &addr) {
    num_msgs += 1;
    Req::Header *header = (Req::Header *)(msg);
    if (header->type == REQ)
    {
      try
      {
        rpc_callbacks_[header->rpc_id](
            *this,
            {.cor_id = header->cor_id, .dest = addr},
            (char *)(header) + sizeof(Req::Header),
            header->payload);
      }
      catch (...)
      {
        LOG(7) << "rpc called failed with rpc id " << header->rpc_id;
        ASSERT(false);
      }
    }
    else if (header->type == REPLY)
    {
      if(sanity_check_reply(header)) {
        memcpy(replies_[header->cor_id].reply_buf,
               (char *)(header) + sizeof(Req::Header),
               header->payload);
        replies_[header->cor_id].reply_buf += header->payload;

        if (--(replies_[header->cor_id].reply_count) == 0)
          {
            s.add(header->cor_id, SUCC);
          }
      }
    }
    else {
      //ASSERT(false) << "receive wrong rpc id:" << (int)header->type;
    }
  });
  if (num_msgs) {
    tsc_spent += read_tsc() - temp;
  }
}

void RPC::spawn_recv(RScheduler &s)
{
  s.emplace(0, 0, [&](std::vector<int> &routine_count) {
    poll_all(s, routine_count);
    return std::make_pair(NOT_READY, 0); // this function should never return
  });
}

} // end namespace rpc

} // end namespace r2
