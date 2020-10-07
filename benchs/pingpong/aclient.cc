#include "lib.hpp"
#include "rlib/rdma_ctrl.hpp"

#include "utils/all.hpp"
#include "thread.hpp"
#include "reporter.hpp"

#include "r2/src/timer.hpp"

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

#define PA 0

volatile bool running = true;

void sigint_handler(int signal) {
  running = false;
}

void pp_callback(RPC &rpc,const Req::Meta &ctx,const char *msg,u32 size) {
  auto &factory = rpc.get_buf_factory();
  char *reply_buf = factory.get_inline();
#if !PA
  ASSERT(rpc.reply_async(ctx,reply_buf,sizeof(u64)) == SUCC);
#endif
}

bool test_func(RPC &rpc,handler_t &h,RScheduler &coro,int thread_id) {

  Addr server_addr = {.mac_id = server_mac_id,.thread_id = thread_id};

  auto id = coro.cur_id();
  auto &factory = rpc.get_buf_factory();
  static thread_local char *send_buf = factory.alloc(4096);
  static thread_local char *reply_buf = (char *)malloc(1024);

  for(uint i = 0;i < FLAGS_batch;++i) {
#if 1
    auto ret = rpc.call_async(
        {.cor_id = id,.dest = server_addr},
        PP_RPC_ID,
        {.send_buf = send_buf + Req::sizeof_header(),.
         len = sizeof(u64),
         .reply_buf = reply_buf,.reply_cnt = (PA == 0)});
#endif
  }
  rpc.flush_pending();
#if !PA
  coro.pause_and_yield(h);
#endif
  return true;
}

int main(int argc,char **argv) {

  gflags::ParseCommandLineFlags(&argc, &argv, true);
  std::signal(SIGINT, sigint_handler);

  /**
   * Since we use RDMA, there will have a lot of startup procedures,
   * including register the memory, and connect QP.
   */
  auto all_devices = RNicInfo::query_dev_names();
  ASSERT(!all_devices.empty()) << "RDMA must be supported.";

  //char *buffer = new char[1 * GB];
  char *buffer = (char *)alloc_huge_page(1 * GB,2 * MB,FLAGS_huge_page);

  /**
   * Bootstrap done, now we start the main loop
   */
  std::vector<Statics>  statics(FLAGS_threads);
  using Worker = Thread<double>;
  std::vector<Worker *> threads;

  for(uint thread_id = 0; thread_id < FLAGS_threads;++thread_id) {

    threads.push_back(
        new Worker([&,thread_id]() {

                     if(FLAGS_bind_core)
                       CoreBinder::bind(thread_id);

                     RNic nic(all_devices[1]);
                     ASSERT(buffer != nullptr) << "failed to allocate buffer.";

                     ASSERT(Helpers::register_mem(ctrl.mr_factory,nic,buffer,1 * GB,thread_id))
                         << "failed to register memory to nic.";

                     auto adapter = Helpers::bootstrap_ud(ctrl.qp_factory,nic,
                                                          make_id(FLAGS_host,FLAGS_port),
                                                          thread_id,thread_id,
                                                          0,thread_id);
                     ASSERT(adapter != nullptr) << "failed to create UDAdapter";

                     Addr server_addr = {.mac_id = server_mac_id,.thread_id = thread_id};
                     while(adapter->connect(server_addr,
                                            ::rdmaio::make_id(FLAGS_server_host,FLAGS_server_port),
                                            thread_id) != SUCC) {
                     }
                     LOG(4) << "connect to server " << FLAGS_server_host << " done";

                     RPC  rpc(adapter);
#if 0
                     /**
                      * There are two ways to create RScheduler.
                      * One is to use a customized routine callback handling,
                      * its just like a normal handling routine.
                      * The routine handling basic invloves RPC handling and
                      * RDMA responses.
                      */
                     RScheduler r([&rpc](handler_t &h,RScheduler &coro) {
                                    while(coro.is_running()) {

                                      // poll the completion events
                                      coro.poll_all();
                                      rpc.poll_all(coro,coro.pending_futures_);

                                      if(coro.next_id() != coro.cur_id()) {
                                        coro.yield_to_next(h);
                                      } else {
                                        // pass
                                      }
                                    }
                                    routine_ret(h,coro);
                                  });
#else
                     RScheduler r;
                     rpc.spawn_recv(r);
#endif

                     rpc.register_callback(PP_RPC_ID,pp_callback);

#if 1
                     r.spawnr([&](handler_t &h,RScheduler &r) {

                                for(uint i = 0;i < FLAGS_concurrency;++i) {
                                  r.spawnr([&](handler_t &h,RScheduler &r) {
                                             // main evaluation loop
                                             while(running) {
                                               r2::compile_fence();
                                            retry:
                                               // try one
                                               if(test_func(rpc,h,r,thread_id)) {
                                                   statics[thread_id].increment(FLAGS_batch);
                                                 } else {
                                                   r.yield_to_next(h);
                                                   goto retry;
                                                 }
                                               r.yield_to_next(h);
                                             }
                                             r.stop_schedule();
                                             routine_ret(h,r);
                                           });
                                }
                                routine_ret(h,r);
                              });
                     r.run();
#endif
                     // dis-connect to server
                     rpc.end_handshake(server_addr);
                     return 0;
                   }));
  }
  for(auto i : threads)
    i->start();

  Reporter::report_thpt(statics,FLAGS_running_time);
  running = false;
#if 1
  for(auto i : threads) {
    //LOG(4) << "thread join with :"  << i->join();
    i->join();
    delete i;
  }
#endif
  delete[] buffer;
  return 0;
}
