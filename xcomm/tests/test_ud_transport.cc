#include <gtest/gtest.h>

#include "../src/transport/rdma_ud_t.hh"

#include "./transport_util.hh"

using namespace xstore::transport;

namespace test {

RCtrl ctrl(8888);

class TransporTest : public testing::Test {
protected:
  virtual void SetUp() override {
    nic_for_recv = RNic::create(RNicInfo::query_dev_names().at(0)).value();
    qp_recv = UD::create(nic_for_recv, QPConfig()).value();

    ctrl.registered_qps.reg("test_server", qp_recv);
    ctrl.start_daemon();
  }

  Arc<RNic> nic_for_recv;
  Arc<RNic> nic_for_send;

  Arc<UD> qp_recv;
};

TEST_F(TransporTest, UDBasic) {

  // prepare UD recv buffer
  auto mem_region = HugeRegion::create(64 * 1024 * 1024).value();
  auto mem = mem_region->convert_to_rmem().value();

  auto handler = RegHandler::create(mem, nic_for_recv).value();
  SimpleAllocator alloc(mem, handler->get_reg_attr().value());

  auto recv_rs = RecvEntriesFactory<SimpleAllocator, 2048, 4096>::create(alloc);
  {
    auto res = qp_recv->post_recvs(*recv_rs, 2048);
    RDMA_ASSERT(res == IOCode::Ok);
  }

  // prepare the sender
  auto nic_for_sender = RNic::create(RNicInfo::query_dev_names().at(0)).value();
  auto qp = UD::create(nic_for_sender, QPConfig()).value();

  UDTransport t;
  ASSERT(t.connect("localhost:8888","test_server",73, qp) == IOCode::Ok) << " connect failure";

  auto ret = t.send(MemBlock((void *)"hello", 6));
  ASSERT(ret == IOCode::Ok);

  int count = 0;

  while (count < 40960) {
    UDRecvTransport<2048> recv(qp_recv, recv_rs);
    for (; recv.has_msgs(); recv.next()) {
      auto msg = recv.cur_msg();
      LOG(0) << "Recv: " << (char *)msg.mem_ptr << " " << count << " ; pending: " << qp->pending_reqs;
      count += 1;
      auto ret = t.send(MemBlock((void *)"hello", 6));
      ASSERT(ret == IOCode::Ok) << "reset error at: " << count << " " << ret.code.name()
                                << "; detailed:" << ret.desc << "; " << qp->pending_reqs;
    }
  }
  LOG(4) << "UD recv passes";
}

} // end namespace test

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
