#include "../pingpong/lib.hpp"
#include "../arc/val.hpp"
#include "rlib/rdma_ctrl.hpp"

#include "utils/all.hpp"
#include "thread.hpp"
#include "../pingpong/reporter.hpp"

#include "r2/src/timer.hpp"
#include "r2/src/futures/rdma_future.hpp"

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
  LOG(4) << "exit";
}

void pp_callback(RPC &rpc,const Req::Meta &ctx,const char *msg,u32 size) {
  auto &factory = rpc.get_buf_factory();
  char *reply_buf = factory.get_inline();
#if !PA
  ASSERT(rpc.reply_async(ctx,reply_buf,sizeof(u64)) == SUCC);
#endif
}

char *buffer = nullptr;

bool test_func(RPC &rpc,RCQP &qp,handler_t &h,RScheduler &coro,int thread_id,char *buf) {

  auto id = coro.cur_id();
  auto &factory = rpc.get_buf_factory();
  //static thread_local char *send_buf = factory.alloc(4096);
  static char *send_buf = factory.alloc(4096 * 100);
  static thread_local char *reply_buf = (char *)malloc(1024);

  for(uint i = 0;i < FLAGS_batch;++i) {
    RdmaFuture::send_wrapper(coro,&qp,id,
                             {.op = IBV_WR_RDMA_READ,
                              .flags = IBV_SEND_SIGNALED,
                              .len = 8,
                              .wr_id = id
                             },
                             //{.local_buf = buffer + 64 * MB * thread_id + id * 4096,
                             {.local_buf = buf,
                             .remote_addr = thread_id * 64 * MB + id * 1024,
                             .imm_data = 0});
  }
  coro.pause_and_yield(h);
  return true;
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
  platforms::VALBinder::bind(0, FLAGS_threads + 1);
  buffer = (char *)alloc_huge_page(1 * GB,2 * MB,FLAGS_huge_page);

  /**
   * Bootstrap done, now we start the main loop
   */
  std::vector<Statics>  statics(FLAGS_threads);
  using Worker = Thread<double>;
  std::vector<Worker *> threads;

  PBarrier barrier(FLAGS_threads + 1);

  for(uint thread_id = 0; thread_id < FLAGS_threads;++thread_id) {

    threads.push_back(
        new Worker([&,thread_id]() {

                     if(FLAGS_bind_core)
                       platforms::VALBinder::bind(0, thread_id);

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
                       sleep(1);
                     }
                     RPC  rpc(adapter);
                     // then we create the QP
                     RemoteMemory::Attr local_mr;
                     RemoteMemory::Attr remote_mr;
                     ASSERT(ctrl.mr_factory.fetch_local_mr(thread_id,local_mr) == SUCC);
                     while(RMemoryFactory::fetch_remote_mr(thread_id,::rdmaio::make_id(FLAGS_server_host,FLAGS_server_port),remote_mr)
                           != SUCC) {
                       sleep(1);
                     }

                     auto qp = new RCQP(nic,remote_mr,local_mr,QPConfig());
                     ctrl.qp_factory.register_rc_qp(thread_id,qp);
                     QPAttr attr;
                     while(QPFactory::fetch_qp_addr(QPFactory::RC,thread_id,make_id(FLAGS_server_host,8888),attr) != SUCC) {
                     }
                     ASSERT(qp->connect(attr,QPConfig()) == SUCC);
#if 0
                     /**
                      * There are two ways to create RScheduler.
                      * One is to use a customized routine callback handling,
                      * its just like a normal handling routine.
                      * The routine handling basic invloves RPC handling and
                      * RDMA responses.
                      */
                     RScheduler r([&rpc](handler_t &h,RScheduler &coro) {
                                    while(running) {

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
                     std::vector<void *> send_bufs(FLAGS_concurrency + 5, nullptr);
                     r.spawnr([&](handler_t &h,RScheduler &r) {

                                for(uint i = 0;i < FLAGS_concurrency;++i) {
                                  r.spawnr([&](handler_t &h,RScheduler &r) {
                                             // main evaluation loop
                                             //char *local_buf = buffer + thread_id * 64 * MB + r.cur_id() * 2 * MB;
                                             //local_buf = (char *)round_up<u64>((u64)local_buf,2 * MB);
                                             //char *local_buf = buffer + thread_id * 4096 + r.cur_id() * 64; // work version
                                             char *local_buf = (char *)AllocatorMaster<>::get_thread_allocator()
                                                               ->alloc(4096,MALLOCX_LG_ALIGN(21));
                                             //char *local_buf = (char *)AllocatorMaster<>::get_thread_allocator()->alloc(8,MALLOCX_LG_ALIGN(7));
                                             while(running) {
                                               r2::compile_fence();
                                            retry:
                                               // try one
                                               if(test_func(rpc,*qp,h,r,thread_id,local_buf)) {
                                                   statics[thread_id].increment(FLAGS_batch);
                                                 } else {
                                                   r.yield_to_next(h);
                                                   goto retry;
                                                 }
                                               r.yield_to_next(h);
                                             }
                                             send_bufs[r.cur_id()] = (void *)local_buf;
                                             r.stop_schedule();
                                             routine_ret(h,r);
                                           });
                                }
                                routine_ret(h,r);
                              });
                     barrier.wait();
                     r.run();
                     std::ostringstream ss; ss << " Thread " << thread_id << " use bufs: ";
                     for(uint i = 0;i < send_bufs.size();++i) {
                       //if(send_bufs[i] != nullptr)
                         //ss <<  (u64)send_bufs[i] - (u64)buffer << ",";
                     }
                     ss << "\n";
                     for(uint i = 0;i < send_bufs.size();++i) {
                       if(send_bufs[i])
                         ss << send_bufs[i] << "," << " %64= " << (u64)(send_bufs[i]) % 64 << " ";
                     }

                     //LOG(4) << ss.str();
#endif
                     // dis-connect to server
                     rpc.end_handshake(server_addr);
                     return 0;
                   }));
  }
  for(auto i : threads)
    i->start();
  barrier.wait();
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
