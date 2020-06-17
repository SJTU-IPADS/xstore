#pragma once

#include "../transport/trait.hh"

#include "./proto.hh"
#include "./reply_station.hh"

namespace xstore {

namespace rpc {

using namespace ::xstore::transport;

/*!
  Usage:
  RPCOp op;
  op.set_msg(some msg).set_rpc_id(xx).set_corid(xx).set_reply(some
  rpc_context).add_arg<T>(T ...); auto ret = rpc.execute(op.finalize());

  \note: set_msg() must be called first!
 */
struct RPCOp {
  Header header;
  MemBlock msg;
  char *cur_ptr = nullptr;

  auto set_msg(const MemBlock &b) -> RPCOp & {
    ASSERT(b.sz >= sizeof(Header));
    this->msg = b;
    this->cur_ptr = (char *)this->msg.mem_ptr + sizeof(Header);
    return *this;
  }

  auto set_rpc_id(const u32 &id) -> RPCOp & {
    this->header.rpc_id = id;
    return *this;
  }

  auto set_corid(const u32 &id) -> RPCOp & {
    this->header.cor_id = id;
    return *this;
  }

  /*!
    Note: the corid must be set using set_corid() before using this function
   */
  auto set_reply(ReplyStation &s, const ReplyEntry &reply) -> RPCOp & {
    s.add_pending_reply(header.cor_id, reply);
    return *this;
  }

  /*!
    \ret: whether add succ
   */
  template <typename T> auto add_arg(const T &arg) -> bool {
    if (unlikely(sizeof(T) + this->cur_sz() > this->msg.sz)) {
      return false;
    }
    *reinterpret_cast<T *>(this->cur_ptr) = arg;
    this->cur_ptr += sizeof(T);
    return true;
  }

  auto finalize() -> RPCOp & {
    this->header.payload = this->cur_sz() - sizeof(Header);
    this->header.type = Req;
    *(reinterpret_cast<Header *>(this->msg.mem_ptr)) = this->header;
    this->msg.sz = this->header.payload + sizeof(Header);
    return *this;
  }

  auto cur_sz() -> usize { return cur_ptr - (char *)msg.mem_ptr; }
};

} // namespace rpc

} // namespace xstore
