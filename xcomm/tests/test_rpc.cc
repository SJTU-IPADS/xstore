#include <gtest/gtest.h>

#include "./transport_util.hh"

#include "../src/rpc/mod.hh"

#include "../src/transport/rdma_ring_t.hh"

namespace test {

using namespace xstore::rpc;

const int max_msg_sz = 4096;
const int ring_sz = max_msg_sz * 128;
const int ring_entry = 128;

// prepare the sender transport
using SendTrait = RRingTransport<ring_entry, ring_sz, max_msg_sz>;
using RecvTrait = RRingRecvTransport<ring_entry, ring_sz, max_msg_sz>;

void test_callback(const Header &rpc_header, const MemBlock &args,
                   SendTrait *replyc) {
  // sanity check the requests
  ASSERT(args.sz == sizeof(u64)) << "args sz:" << args.sz;
  auto val = *((u64 *)args.mem_ptr);
  ASSERT(val == 73);

  // send the reply
  RPCOp op;
  char reply_buf[64];
  op.set_msg(MemBlock(reply_buf, 64)).set_reply().set_corid(2).add_arg<u64>(73 + 1);
  auto ret = op.execute(replyc);
  ASSERT(ret == IOCode::Ok);
}

TEST(RPC, basic) {

  RCtrl ctrl(8888);
  RingManager<128> rm(ctrl);

  RPCCore<SendTrait,RecvTrait> rpc(12);
  auto rpc_id = rpc.reg_callback(test_callback);

  auto nic = RNic::create(RNicInfo::query_dev_names().at(0)).value();
  RDMA_ASSERT(ctrl.opened_nics.reg(0, nic));

  auto mem_region = HugeRegion::create(64 * 1024 * 1024).value();
  // auto mem_region = DRAMRegion::create(64 * 1024 * 1024).value();
  auto mem = mem_region->convert_to_rmem().value();
  auto handler = RegHandler::create(mem, nic).value();

  auto alloc = Arc<SimpleAllocator>(
      new SimpleAllocator(mem, handler->get_reg_attr().value()));

  ctrl.start_daemon();

  // The transport <SendTrait, RecvTrait> at the RPC sender
  std::unique_ptr<SendTrait> sender = nullptr;
  std::unique_ptr<RecvTrait> sender_recv = nullptr;
  {
    /*
      init the recv point at the sender, inorder to receive the reply
     */
    auto recv_cq_res = ::rdmaio::qp::Impl::create_cq(nic, 128);
    RDMA_ASSERT(recv_cq_res == IOCode::Ok);
    auto recv_cq = std::get<0>(recv_cq_res.desc);

    auto receiver = RecvFactory<ring_entry, ring_sz, max_msg_sz>::create(
                        rm, std::to_string(0), recv_cq, alloc)
                        .value();
    // init
    sender = std::make_unique<SendTrait>(73, nic, QPConfig(), recv_cq, alloc);
    sender_recv = std::make_unique<RecvTrait>(receiver);
    receiver->reg_channel(sender->core);
  }

  // the transport <_, RecvTrait> at the RPC receiver
  std::unique_ptr<RecvTrait> recv = nullptr;
  {
    /*
      init the receive
     */
    auto recv_cq_res = ::rdmaio::qp::Impl::create_cq(nic, 128);
    RDMA_ASSERT(recv_cq_res == IOCode::Ok);
    auto recv_cq1 = std::get<0>(recv_cq_res.desc);
#if 1
    auto receiver1 = RecvFactory<ring_entry, ring_sz, max_msg_sz>::create(
                         rm, std::to_string(73), recv_cq1, alloc)
                         .value();
    recv = std::make_unique<RecvTrait>(receiver1);
#endif
  }

  auto res_c =
      sender->connect("localhost:8888", std::to_string(73), 0, QPConfig());
  ASSERT(res_c == IOCode::Ok) << "res_c error: " << res_c.code.name();

  LOG(4) << "RPC init done";

  // start call
  // 1. prepare the bufs
  auto send_buf = std::get<0>(alloc->alloc_one(4096).value());
  char reply_buf[1024];

  // 2. prepare the op
  RPCOp op;
  op.set_msg(MemBlock(send_buf, 4096))
      .set_req()
      .set_rpc_id(rpc_id)
      .set_corid(2)
      .add_one_reply(rpc.reply_station, {.mem_ptr = reply_buf, .sz = 1024 })
      .add_arg<u64>(73);
  ASSERT(sender.get() != nullptr);
  auto ret = op.execute(sender.get());
  ASSERT(ret == IOCode::Ok);

  sleep(1);

  usize counter = 0;
  while (true) {
    // recv rpc calls
    counter += rpc.recv_event_loop(recv.get());

    // recv reply
    rpc.recv_event_loop(sender_recv.get());

    if (rpc.reply_station.cor_ready(2)) {
      // we have received the reply
      RPCOp op;
      op.set_msg(MemBlock(send_buf, 4096))
          .set_req()
          .set_rpc_id(rpc_id)
          .set_corid(2)
          .add_one_reply(rpc.reply_station, {.mem_ptr = reply_buf, .sz = 1024})
          .add_arg<u64>(73);
      ASSERT(sender.get() != nullptr);
      auto ret = op.execute(sender.get());
      ASSERT(ret == IOCode::Ok);
    }

    if (counter >= 102400) {
      break;
    }
  }
  ASSERT_LE(counter, 102400);
}
} // namespace test

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
