#include <gflags/gflags.h>

#include "../tests/transport_util.hh"

#include "../src/rpc/mod.hh"

#include "../src/transport/rdma_ud_t.hh"

#include "../../deps/r2/src/thread.hh"

namespace bench {

using namespace test;

using namespace xstore::rpc;
using namespace xstore::transport;

// prepare the sender transport
using SendTrait = UDTransport;
using RecvTrait = UDRecvTransport<2048>;
using SManager = UDSessionManager<2048>;

}

using namespace bench;

DEFINE_int64(threads, 1, "num client thread used");
DEFINE_int64(coros, 1, "num client coroutine used per threads");
DEFINE_string(addr, "localhost:8888", "server address");

using XThread = ::r2::Thread<usize>;

int main(int argc, char **argv) {

  gflags::ParseCommandLineFlags(&argc, &argv, true);

  std::vector<std::unique_ptr<XThread>> workers;

  for (uint thread_id = 0; thread_id < FLAGS_threads; ++thread_id) {
    workers.push_back(std::move(std::make_unique<XThread>([thread_id]() -> usize {
      auto nic_for_sender =
          RNic::create(RNicInfo::query_dev_names().at(0)).value();
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

      UDTransport sender;
      ASSERT(sender.connect(FLAGS_addr, "b" + std::to_string(thread_id), thread_id, qp) ==
             IOCode::Ok)
          << " connect failure at addr: " << FLAGS_addr;

      RPCCore<SendTrait, RecvTrait, SManager> rpc(12);
      auto send_buf = std::get<0>(alloc1.alloc_one(4096).value());
      ASSERT(send_buf != nullptr);
      auto lkey = handler1->get_reg_attr().value().key;

      memset(send_buf, 0, 4096);
      // first we send the connect transport
      auto conn_op = RPCOp::get_connect_op(MemBlock(send_buf, 2048),
                                           sender.get_connect_data().value());
      auto ret = conn_op.execute_w_key(&sender, lkey);
      ASSERT(ret == IOCode::Ok);

      UDRecvTransport<2048> recv_s(qp, recv_rs_at_send);

      SScheduler ssched;
      rpc.reg_poll_future(ssched, &recv_s);

      // TODO: spawn coroutines for sending the reqs
      for (uint i = 0; i < FLAGS_coros; ++i) {
        ssched.spawn([](R2_ASYNC) {
          ASSERT(false);
          R2_RET;
        });
      }
      ssched.run();

      return 0;
    })));
  }

  for (auto &w : workers) {
    w->start();
  }

  for (auto &w : workers) {
    w->join();
  }

  LOG(4) << "rpc server finishes";
  return 0;
}