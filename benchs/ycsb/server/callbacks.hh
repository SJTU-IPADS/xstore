#pragma once

#include "../proto.hh"
#include "../schema.hh"

#include "../../../xcomm/src/rpc/mod.hh"
#include "../../../xcomm/src/transport/rdma_ud_t.hh"

namespace xstore {

using namespace xstore::rpc;
using namespace xstore::transport;

using SendTrait = UDTransport;

extern DBTree db;
extern std::unique_ptr<XCache> cache;
extern std::vector<XCacheTT> tts;

extern u64 model_buf;
extern u64 tt_buf;

void meta_callback(const Header &rpc_header, const MemBlock &args,
                   SendTrait *replyc) {
  // sanity check the requests
  ASSERT(args.sz == sizeof(u64));

  // prepare the reply
  ASSERT(cache != nullptr);
  char reply_buf[64];

  auto dispatcher = cache->first_layer.serialize();
  ReplyMeta meta = {
      .dispatcher_sz = static_cast<u32>(dispatcher.size()),
      .total_sz = static_cast<u32>(buf_end - model_buf),
      .model_buf = model_buf,
      .tt_buf = tt_buf,
  };
  ASSERT(sizeof(ReplyMeta) + dispatcher.size() + sizeof(Header) <= 64)
      << sizeof(ReplyMeta) << " " << dispatcher.size();

  // send
  RPCOp op;
  ASSERT(op.set_msg(MemBlock(reply_buf, 64)).set_reply().add_arg(meta));
  ASSERT(op.add_opaque(dispatcher));
  op.set_corid(rpc_header.cor_id);

  auto ret = op.execute(replyc);
  ASSERT(ret == IOCode::Ok);
}

/*!
  handle Get() request using RPC
 */
void get_callback(const Header &rpc_header, const MemBlock &args,
                   SendTrait *replyc) {
  // sanity check the requests
  ASSERT(args.sz == sizeof(u64));

  u64 key = *(reinterpret_cast<u64 *>(args.mem_ptr));

  // get()
  auto value = db.get(XKey(key)).value();

  char reply_buf[64];

  // send the reply
  RPCOp op;
  ASSERT(op.set_msg(MemBlock(reply_buf, 64)).set_reply().add_arg(value));
  op.set_corid(rpc_header.cor_id);

  auto ret = op.execute(replyc);
  ASSERT(ret == IOCode::Ok);
}

} // namespace xstore
