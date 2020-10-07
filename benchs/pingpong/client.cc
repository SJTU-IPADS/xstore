#include "lib.hpp"
#include "r2/src/scheduler.hpp"
#include "rlib/rdma_ctrl.hpp"

#include "thread.hpp"
#include "utils/all.hpp"

#include "r2/src/futures/rdma_future.hpp"
#include "r2/src/timer.hpp"

#include <csignal>
#include <gflags/gflags.h>

using namespace rdmaio;
using namespace fstore::utils;
using namespace pingpong;

RdmaCtrl ctrl(8888);

DEFINE_int64(port, 8888, "My port used.");
DEFINE_string(host, "localhost", "My host used.");
DEFINE_string(server_host, "localhost", "Server host used.");
DEFINE_int64(server_port, 8888, "Server port used.");

DEFINE_int64(client_id, 0, "The unique id of clients.");

volatile bool running = true;

void
sigint_handler(int signal)
{
  running = false;
}

int
main(int argc, char** argv)
{

  gflags::ParseCommandLineFlags(&argc, &argv, true);
  std::signal(SIGINT, sigint_handler);

  /**
   * Since we use RDMA, there will have a lot of startup procedures,
   * including register the memory, and connect QP.
   */
  auto all_devices = RNicInfo::query_dev_names();
  ASSERT(!all_devices.empty()) << "RDMA must be supported.";

  RNic nic(all_devices[0]);
  RNic nic2(all_devices[1]);

  char* buffer = new char[1 * GB];
  ASSERT(buffer != nullptr) << "failed to allocate buffer.";

  ASSERT(Helpers::register_mem(ctrl.mr_factory, nic, buffer, 1 * GB, mr_id))
    << "failed to register memory to nic.";
  ASSERT(
    Helpers::register_mem(ctrl.mr_factory, nic2, buffer, 1 * GB, mr_id + 1))
    << "failed to register memory to nic.";

  auto adapter = Helpers::bootstrap_ud(
    ctrl.qp_factory, nic, make_id(FLAGS_host, FLAGS_port), qp_id, mr_id, 0, 73);
  ASSERT(adapter != nullptr) << "failed to create UDAdapter";

  Addr server_addr = { .mac_id = server_mac_id, .thread_id = 1 };
  while (
    adapter->connect(server_addr,
                     ::rdmaio::make_id(FLAGS_server_host, FLAGS_server_port),
                     qp_id) != SUCC) {
  }
  LOG(4) << "connect to server " << FLAGS_server_host << " done";

  /**
   * Bootstrap done, now we start the main loop
   */
  RScheduler r;
  RPC rpc(adapter);
  rpc.spawn_recv(r);

  RemoteMemory::Attr local_mr;
  auto ret = ctrl.mr_factory.fetch_local_mr(mr_id, local_mr);
  RDMA_ASSERT(ret == SUCC);

  RemoteMemory::Attr local_mr1;
  ret = ctrl.mr_factory.fetch_local_mr(mr_id + 1, local_mr1);
  RDMA_ASSERT(ret == SUCC);

  RemoteMemory::Attr remote_mr;
  while (RMemoryFactory::fetch_remote_mr(
           mr_id, std::make_tuple(FLAGS_server_host, 8888), remote_mr) !=
         SUCC) {
  }

  RemoteMemory::Attr remote_mr1;
  while (RMemoryFactory::fetch_remote_mr(
           mr_id + 1, std::make_tuple(FLAGS_server_host, 8888), remote_mr1) !=
         SUCC) {
  }

  auto qp = new RCQP(nic, remote_mr, local_mr, QPConfig());
  auto qp2 = new RCQP(nic2, remote_mr1, local_mr1, QPConfig());
  QPAttr attr;
  ctrl.qp_factory.register_rc_qp(0, qp);
  ctrl.qp_factory.register_rc_qp(1, qp2);

  while (ctrl.qp_factory.fetch_qp_addr(
           QPFactory::RC, 0, std::make_tuple(FLAGS_server_host, 8888), attr) !=
         SUCC) {
  }
  ASSERT(qp->connect(attr, QPConfig()) == SUCC);
  while (ctrl.qp_factory.fetch_qp_addr(
           QPFactory::RC, 1, std::make_tuple(FLAGS_server_host, 8888), attr) !=
         SUCC) {
  }
  ASSERT(qp2->connect(attr, QPConfig()) == SUCC);
  LOG(4) << "all QP connects done";

  r.spawnr([&](handler_t& h, RScheduler& r) {
    //    auto ret = rpc.start_handshake(server_addr, r, h);
    //    ASSERT(ret == SUCC);
    //    LOG(4) << "server handshake done.";

    r.spawnr([&](R2_ASYNC) {
      auto id = R2_COR_ID();

      auto factory = rpc.get_buf_factory();
      //      auto send_buf = factory.get_inline_buf();
      char* send_buf = buffer;

      u64 done(0);
      Timer t;
      // main pingpong loop
      u64 counter = 1;
      LOG(4) << "client start send req";
      while (counter < 1024 * 1024 * 10) {
        *((u64*)send_buf) = counter;
        r2::compile_fence();
        RdmaFuture::send_wrapper(R2_EXECUTOR,
                                 qp,
                                 id,
                                 { .op = IBV_WR_RDMA_WRITE,
                                   .flags = IBV_SEND_SIGNALED | IBV_SEND_INLINE,
                                   .len = sizeof(u64),
                                   .wr_id = id },
                                 { .local_buf = send_buf,
                                   .remote_addr = (counter + 1) * sizeof(u64),
                                   .imm_data = 0 });
        R2_PAUSE_AND_YIELD;
        //        LOG(4) << "Seond one done";
        //        sleep(1);
        *((u64*)send_buf + 1) = counter;
        r2::compile_fence();
        RdmaFuture::send_wrapper(R2_EXECUTOR,
                                 qp2,
                                 id,
                                 { .op = IBV_WR_RDMA_WRITE,
                                   .flags = IBV_SEND_SIGNALED | IBV_SEND_INLINE,
                                   .len = sizeof(u64),
                                   .wr_id = id },
                                 { .local_buf = send_buf + sizeof(u64),
                                   .remote_addr = 0,
                                   .imm_data = 0 });
        R2_PAUSE_AND_YIELD
        r2::compile_fence();
        counter += 1;
      }
      LOG(4) << "done";
      routine_ret(h, r);
    });

    routine_ret(h, r);
  });

  r.run();
  // dis-connect to server
  //  rpc.end_handshake(server_addr);

  delete[] buffer;
  return 0;
}
