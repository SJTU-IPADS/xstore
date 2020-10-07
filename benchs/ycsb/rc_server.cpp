#include "../pingpong/lib.hpp"
#include "rlib/rdma_ctrl.hpp"

#include "utils/all.hpp"
#include "thread.hpp"

#include "r2/src/timer.hpp"
#include "r2/src/futures/rdma_future.hpp"

#include "../arc/val.hpp"

#include <gflags/gflags.h>
#include <csignal>

using namespace rdmaio;
using namespace fstore::utils;
using namespace pingpong;

RdmaCtrl ctrl(8888);

DEFINE_int64(port,8888,"My port used.");
DEFINE_string(host,"localhost","My host used.");
DEFINE_string(server_host,"localhost","Server host used.");
DEFINE_int64(server_port,8888,"Server port used.");

DEFINE_int64(client_id,0,"The unique id of clients.");
DEFINE_int64(threads,1,"Number of threads used.");
DEFINE_int64(running_time,10,"Number of time to run (in seconds).");
DEFINE_uint64(concurrency,1,"Number of coroutine per thread.");
DEFINE_uint64(batch,1,"Number of batch requests.");

DEFINE_bool(huge_page,true,"Use hugepage for RDMA's buf allocation.");
DEFINE_bool(bind_core,true,"Bind thread to a specific core for better performance.");

DEFINE_string(client,"val01","Client host");

#define PA 0

volatile bool running = true;

void sigint_handler(int signal) {
  running = false;
  LOG(4) << "exit";
}

void pp_callback(RPC &rpc,const Req::Meta &ctx,const char *msg,u32 size) {
  auto &factory = rpc.get_buf_factory();
  char *reply_buf = factory.get_inline();
#if !PA
  ASSERT(rpc.reply_async(ctx,reply_buf,sizeof(u64)) == SUCC);
#endif
}

int main(int argc,char **argv) {

  gflags::ParseCommandLineFlags(&argc, &argv, true);
  //std::signal(SIGINT, sigint_handler);

  /**
   * Since we use RDMA, there will have a lot of startup procedures,
   * including register the memory, and connect QP.
   */
  auto all_devices = RNicInfo::query_dev_names();
  ASSERT(!all_devices.empty()) << "RDMA must be supported.";

  //char *buffer = new char[1 * GB];
  platforms::VALBinder::bind(0, 0);
  char *buffer = (char *)alloc_huge_page(16 * GB,2 * MB,FLAGS_huge_page);

  /**
   * Bootstrap done, now we start the main loop
   */
  using Worker = Thread<double>;
  std::vector<Worker *> threads;
  PBarrier barrier(FLAGS_threads + 1);

  for(uint thread_id = 0; thread_id < FLAGS_threads;++thread_id) {

    threads.push_back(
        new Worker([&,thread_id]() {

                     if(FLAGS_bind_core)
                       //CoreBinder::bind(thread_id);
                       platforms::VALBinder::bind(0, thread_id);

                     ASSERT(all_devices.size() >= 2);
                     RNic nic(all_devices[1]);
                     ASSERT(buffer != nullptr) << "failed to allocate buffer.";

                     ASSERT(Helpers::register_mem(ctrl.mr_factory,nic,buffer,16 * GB,thread_id))
                         << "failed to register memory to nic.";

                     auto adapter = Helpers::bootstrap_ud(ctrl.qp_factory,nic,
                                                          make_id(FLAGS_host,FLAGS_port),
                                                          thread_id,thread_id,
                                                          0,thread_id);
                     ASSERT(adapter != nullptr) << "failed to create UDAdapter";

                     RPC  rpc(adapter);
                     // then we create the QP
                     RemoteMemory::Attr local_mr;
                     ASSERT(ctrl.mr_factory.fetch_local_mr(thread_id,local_mr) == SUCC);
                     auto qp = new RCQP(nic,local_mr,local_mr,QPConfig());
                     QPAttr attr;
                     while(QPFactory::fetch_qp_addr(QPFactory::RC,thread_id,make_id(FLAGS_client,8888),attr) != SUCC) { sleep(1);}
                     ctrl.qp_factory.register_rc_qp(thread_id,qp);

                     ASSERT(qp->connect(attr,QPConfig()) == SUCC);

                     RScheduler r;
                     rpc.spawn_recv(r);
                     r.run();
                     return 0;
                   }));
  }

  for(auto i : threads)
    i->start();

  LOG(4) << "server enter main sleep loop";
  while(1) {
    sleep(1);
  }

  for(auto i : threads) {
    //LOG(4) << "thread join with :"  << i->join();
    i->join();
    delete i;
  }
  delete[] buffer;
  return 0;
}
