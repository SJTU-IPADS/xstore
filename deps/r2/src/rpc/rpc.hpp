#pragma once

#include "../common.hpp"
#include "../scheduler.hpp"
#include "rpc_data.hpp"
#include "buf_factory.hpp"

namespace r2 {

namespace rpc {

/**
 * First, we reserve 3 RPC for my iternal usage
 */
const int RESERVED_RPC_ID = 3;

/** The RPC callback takes 6 parameters:
 *  1. Rpc
 *  2. RPC context (sid,tid,cid)
 *  3. a pointer to the msg
 *  4. an extra(typically not used argument)
 */
class RPCHandler;
class RPC {
  friend class RPCHandler;
  /*!
    An RPC callback takes the following parameter:
    \param RPC, a reference to the RPC handler itself.
    \param ctx: the sender's ctx, including sender's corid and its address.
    \param msg: a pointer to the in-coming message
    \param size: the buffer of the message size
   */
  using rpc_func_t =  std::function<void(RPC &,const Req::Meta &ctx,const char *,u32)>;
 public:

  //u64 cur_data_transfered = 0;
  explicit RPC(std::shared_ptr<MsgProtocol> msg_handler);

  void register_callback(int rpc_id, rpc_func_t callback);

  /**
   * Call and reply methods.
   * Meta-data reserved for each message.
   * It includes a header, and some implementation specific padding
   */
  rdmaio::IOStatus call(const Req::Meta &context, int rpc_id, const Req::Arg &arg);

  rdmaio::IOStatus reply(const Req::Meta &context, char *reply,int size);

  rdmaio::IOStatus call_async(const Req::Meta &context, int rpc_id, const Req::Arg &arg);

  rdmaio::IOStatus reply_async(const Req::Meta &context, char *reply,int size);

  inline rdmaio::IOStatus flush_pending() {
    return msg_handler_->flush_pending();
  }

  /*!
   * Handshake functions.
   * Basically, a start hand-shake which has connectinfo,
   * allow this endpoint to create connect information to the sender.
   * a stop hand-shake delete the sender's information from the server.
   */
  rdmaio::IOStatus start_handshake(const Addr &dest,RScheduler &s,handler_t &h);

  rdmaio::IOStatus end_handshake(const Addr &dest);
  // xxx

  /**
   * Return a future, so that the scheduler can poll it for receiving messages
   */
  void spawn_recv(RScheduler &s);

  void poll_all(RScheduler &s,std::vector<int> &routine_count);

  BufFactory alloc_buf_factory() {
    return BufFactory(padding_ + sizeof(Req::Header));
  }

  inline int reserved_header_sz() const {
    return padding_ + sizeof(Req::Header);
  }

  BufFactory &get_buf_factory() {
    return buf_factory_;
  }

  int get_rpc_padding() const {
    return padding_ + sizeof(Req::Header);
  }

 private:
  std::shared_ptr<MsgProtocol>        msg_handler_;
  std::vector<rpc_func_t>             rpc_callbacks_;
  const int                           padding_ = 0;

  // replies
  std::vector<Reply>                  replies_;

  BufFactory                          buf_factory_;

  bool sanity_check_reply(const Req::Header *header);

  u64 tsc_spent = 0;

public:
  // cycles used to process tsc since the last cally
  u64 report_and_reset_processed() {
    auto ret = tsc_spent;
    tsc_spent = 0;
    return ret;
  }

  DISABLE_COPY_AND_ASSIGN(RPC);
}; // end class RPC

} // end namespace rpc

} // end namespace r2
