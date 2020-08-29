#include <gflags/gflags.h>

#include "../tests/transport_util.hh"

#include "../src/rpc/mod.hh"

#include "../src/transport/rdma_ud_t.hh"

#include "../../deps/r2/src/thread.hh"

namespace bench {

using namespace xstore::rpc;
using namespace xstore::transport;
using namespace test;

// prepare the sender transport
using SendTrait = UDTransport;
using RecvTrait = UDRecvTransport<2048>;
using SManager = UDSessionManager<2048>;

// this benchmark simply returns a null reply
void bench_callback(const Header &rpc_header, const MemBlock &args,
                   SendTrait *replyc) {
  // sanity check the requests
  ASSERT(args.sz == sizeof(u64)) << "args sz:" << args.sz;
  auto val = *((u64 *)args.mem_ptr);
  ASSERT(val == 73);
  // LOG(4) << "in tes tcallback !"; sleep(1);

  // send the reply
  RPCOp op;
  char reply_buf[64];
  op.set_msg(MemBlock(reply_buf, 64))
      .set_reply()
      .set_corid(rpc_header.cor_id)
      .add_arg<u64>(73 + 1);
  auto ret = op.execute(replyc);
  ASSERT(ret == IOCode::Ok);
}

volatile bool running = true;

// this benchmark simply returns a null reply
void bench_stop(const Header &rpc_header, const MemBlock &args,
                    SendTrait *replyc) {
  running = false;
}

RCtrl ctrl(8888);

}

using namespace bench;

DEFINE_int64(threads, 1, "num server thread used");

using XThread = ::r2::Thread<usize>;

int main(int argc, char **argv) {

  gflags::ParseCommandLineFlags(&argc, &argv, true);

  std::vector<std::unique_ptr<XThread>> workers;

  for (uint i = 0; i < FLAGS_threads; ++i) {
    workers.push_back(std::move(std::make_unique<XThread>([i]() -> usize {
      auto thread_id = i;
      auto nic_for_recv = RNic::create(RNicInfo::query_dev_names().at(0)).value();
      auto qp_recv = UD::create(nic_for_recv, QPConfig()).value();

      // some bootstrap code
      // prepare UD recv buffer
      auto mem_region = HugeRegion::create(64 * 1024 * 1024).value();
      auto mem = mem_region->convert_to_rmem().value();

      auto handler = RegHandler::create(mem, nic_for_recv).value();
      SimpleAllocator alloc(mem, handler->get_reg_attr().value());

      auto recv_rs_at_recv =
          RecvEntriesFactory<SimpleAllocator, 2048, 4096>::create(alloc);
      {
        auto res = qp_recv->post_recvs(*recv_rs_at_recv, 2048);
        RDMA_ASSERT(res == IOCode::Ok);
      }

      ctrl.registered_qps.reg("b" + std::to_string(thread_id), qp_recv);
      LOG(4) << "server thread #" << thread_id << " started!";

      RPCCore<SendTrait, RecvTrait, SManager> rpc(12);
      rpc.reg_callback(bench_callback);
      rpc.reg_callback(bench_stop);

      UDRecvTransport<2048> recv(qp_recv, recv_rs_at_recv);

      usize epoches = 0;
      while (running) {
        r2::compile_fence();
        rpc.recv_event_loop(&recv);
      }

      return 0;
    })));
  }

  ctrl.start_daemon();

  for (auto &w : workers) {
    w->start();
  }

  for (auto &w : workers) {
    w->join();
  }

  LOG(4) << "rpc server finishes";
  return 0;
}
