#include <gtest/gtest.h>

#include "./transport_util.hh"

#include "../src/rpc/mod.hh"

#include "../src/transport/rdma_ud_t.hh"

namespace test {

using namespace xstore::rpc;
using namespace xstore::transport;

// prepare the sender transport
using SendTrait = UDTransport;
using RecvTrait = UDRecvTransport<2048>;
using SManager = UDSessionManager<2048>;

void test_callback(const Header &rpc_header, const MemBlock &args,
                   SendTrait *replyc) {
  // sanity check the requests
  ASSERT(args.sz == sizeof(u64)) << "args sz:" << args.sz;
  auto val = *((u64 *)args.mem_ptr);
  ASSERT(val == 73);
  //LOG(4) << "in tes tcallback !"; sleep(1);

  // send the reply
  RPCOp op;
  char reply_buf[64];
  op.set_msg(MemBlock(reply_buf, 64))
      .set_reply()
      .set_corid(2)
      .add_arg<u64>(73 + 1);
  auto ret = op.execute(replyc);
  ASSERT(ret == IOCode::Ok);
}

RCtrl ctrl(8888);

class RPC : public testing::Test {
protected:
  virtual void SetUp() override {
    nic_for_recv = RNic::create(RNicInfo::query_dev_names().at(0)).value();
    qp_recv = UD::create(nic_for_recv, QPConfig()).value();

    ctrl.registered_qps.reg("test_server", qp_recv);
  }

  Arc<RNic> nic_for_recv;
  Arc<RNic> nic_for_send;

  Arc<UD> qp_recv;
};

TEST_F(RPC, basic) {

  // some bootstrap code
  // prepare UD recv buffer
  auto mem_region = HugeRegion::create(64 * 1024 * 1024).value();
  auto mem = mem_region->convert_to_rmem().value();

  auto handler = RegHandler::create(mem, nic_for_recv).value();
  SimpleAllocator alloc(mem, handler->get_reg_attr().value());

  auto recv_rs_at_recv = RecvEntriesFactory<SimpleAllocator, 2048, 4096>::create(alloc);
  {
    auto res = qp_recv->post_recvs(*recv_rs_at_recv, 2048);
    RDMA_ASSERT(res == IOCode::Ok);
  }

  // prepare the sender
  auto nic_for_sender = RNic::create(RNicInfo::query_dev_names().at(0)).value();
  auto qp = UD::create(nic_for_sender, QPConfig()).value();

  auto mem_region1 = HugeRegion::create(64 * 1024 * 1024).value();
  auto mem1 = mem_region1->convert_to_rmem().value();
  auto handler1 = RegHandler::create(mem1, nic_for_sender).value();
  SimpleAllocator alloc1(mem1, handler1->get_reg_attr().value());
  auto recv_rs_at_send =
      RecvEntriesFactory<SimpleAllocator, 2048, 4096>::create(alloc1);
  {
    auto res = qp->post_recvs(*recv_rs_at_send, 2048);
    RDMA_ASSERT(res == IOCode::Ok);
  }

  ctrl.start_daemon();

  UDTransport sender;
  ASSERT(sender.connect("localhost:8888", "test_server", 73, qp) == IOCode::Ok)
    << " connect failure";

  // the trait-independent test body
  RPCCore<SendTrait, RecvTrait, SManager> rpc(12);
  auto rpc_id = rpc.reg_callback(test_callback);

  // start call
  // 1. prepare the bufs
  auto send_buf = std::get<0>(alloc1.alloc_one(4096).value());
  ASSERT(send_buf != nullptr);
  auto lkey = handler1->get_reg_attr().value().key;
  char reply_buf[1024];

  memset(send_buf, 0, 4096);
  // first we send the connect data
  auto conn_op = RPCOp::get_connect_op(MemBlock(send_buf,2048),
                                       sender.get_connect_data().value());
  auto ret = conn_op.execute_w_key(&sender, lkey);
  ASSERT(ret == IOCode::Ok);
#if 1
  // 2. prepare the op
  RPCOp op;
  op.set_msg(MemBlock((char *)send_buf + 2048, 2048))
      .set_req()
      .set_rpc_id(rpc_id)
      .set_corid(2)
      .add_one_reply(rpc.reply_station, {.mem_ptr = reply_buf, .sz = 1024})
      .add_arg<u64>(73);
  ret = op.execute_w_key(&sender,lkey);
  ASSERT(ret == IOCode::Ok);
#endif
  sleep(1);

  usize counter = 0;
  UDRecvTransport<2048> recv(qp_recv, recv_rs_at_recv);
  UDRecvTransport<2048> recv_s(qp, recv_rs_at_send);

  LOG(4) << "start main loop 1";

  while (true) {
    // recv rpc calls
    counter += rpc.recv_event_loop(&recv);
    //LOG(4) << "execute: " << counter << " rpcs"; sleep(1);
    // recv reply
    rpc.recv_event_loop(&recv_s);
#if 1
    if (rpc.reply_station.cor_ready(2)) {
      //LOG(4) << "coroutine ready!";
      // we have received the reply
      RPCOp op;
      op.set_msg(MemBlock((char *)send_buf + 2048, 2048))
          .set_req()
          .set_rpc_id(rpc_id)
          .set_corid(2)
          .add_one_reply(rpc.reply_station, {.mem_ptr = reply_buf, .sz = 1024})
          .add_arg<u64>(73);
      ret = op.execute_w_key(&sender, lkey);
      ASSERT(ret == IOCode::Ok);
    }
#endif
    if (counter >= 102400) {
      break;
    }
  }
  ASSERT_LE(counter, 102400);
  LOG(4) << "done; total: " << counter << " processed";
}
} // namespace test

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
