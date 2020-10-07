#include "new_lib.hpp"
#include "rlib/rdma_ctrl.hpp"

#include "reporter.hpp"
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
DEFINE_int64(threads, 1, "Number of threads used.");
DEFINE_int64(running_time, 10, "Number of time to run (in seconds).");
DEFINE_uint64(concurrency, 1, "Number of coroutine per thread.");
DEFINE_uint64(batch, 1, "Number of batch requests.");

DEFINE_bool(huge_page, true, "Use hugepage for RDMA's buf allocation.");
DEFINE_bool(bind_core, true,
            "Bind thread to a specific core for better performance.");

#define PA 0

volatile bool running = true;

void sigint_handler(int signal) {
  running = false;
  LOG(4) << "exit" << std::endl;
  exit(-1);
}

void pp_callback(RPC &rpc, const Req::Meta &ctx, const char *msg, u32 size) {
  auto &factory = rpc.get_buf_factory();
  char *reply_buf = factory.get_inline();
#if !PA
  ASSERT(rpc.reply_async(ctx, reply_buf, sizeof(u64)) == SUCC);
#endif
}

bool test_func(RCQP &qp, int thread_id, R2_ASYNC) {

  auto id = R2_COR_ID(); // current coroutine id

  static thread_local char *send_buf =
      (char *)AllocatorMaster<0>::get_thread_allocator()->alloc(4096);
  // static thread_local char *send_buf = "Hello";
  char tmp[] = "hello";

  strcpy(send_buf, tmp);

  static thread_local char *reply_buf = (char *)malloc(1024);

  for (uint i = 0; i < FLAGS_batch; ++i) {
    RdmaFuture::send_wrapper(R2_EXECUTOR, &qp, id,
                             {.op = IBV_WR_RDMA_WRITE,
                              .flags = IBV_SEND_SIGNALED,
                              .len = 8,
                              .wr_id = id},
                             //{.local_buf = send_buf + id * 128,
                             {.local_buf = send_buf,
                              // .remote_addr = thread_id * 64 * MB + id * 1024,
                              .remote_addr = 64,
                              .imm_data = 0});
  }

  // pause the current coroutine to wait for remote results
  R2_PAUSE_AND_YIELD;

  // std::cout << "value " << std::string((char *)(send_buf + 64)) << std::endl;
  // the read requests must done
  return true;
}

int main(int argc, char **argv) {

  gflags::ParseCommandLineFlags(&argc, &argv, true);
  std::signal(SIGINT, sigint_handler);

  /**
   * Since we use RDMA, there will have a lot of startup procedures,
   * including register the memory, and connect QP.
   */
  auto all_devices = RNicInfo::query_dev_names();
  ASSERT(!all_devices.empty()) << "RDMA must be supported.";

  // register a buffer so that RDMA can read/write
  char *buffer = (char *)alloc_huge_page(1 * GB, 2 * MB, FLAGS_huge_page);

  /**
   * Bootstrap done, now we start the main loop
   */
  std::vector<Statics> statics(FLAGS_threads);
  using Worker = Thread<double>;
  std::vector<Worker *> threads;

  for (uint thread_id = 0; thread_id < FLAGS_threads; ++thread_id) {

    threads.push_back(new Worker([&, thread_id]() {
      // This is the thread main body
      if (FLAGS_bind_core)
        CoreBinder::bind(thread_id);

      RNic nic(all_devices[1]);
      ASSERT(buffer != nullptr) << "failed to allocate buffer.";

      ASSERT(Helpers::register_mem(ctrl.mr_factory, nic, buffer, 1 * GB,
                                   thread_id))
          << "failed to register memory to nic.";

      // then we create the QP
      RemoteMemory::Attr local_mr;
      ASSERT(ctrl.mr_factory.fetch_local_mr(thread_id, local_mr) == SUCC);
      auto qp = new RCQP(nic, local_mr, local_mr, QPConfig());
      auto attr = qp->get_attr();
      ASSERT(qp->connect(attr, QPConfig()) == SUCC);
      // create QP done

      // The scheduler to run all coroutines
      RScheduler r;
#if 1
      // use coroutines to execute functions
      r.spawnr([&](R2_ASYNC) {
        for (uint i = 0; i < FLAGS_concurrency; ++i) {
          R2_EXECUTOR.spawnr([&](R2_ASYNC) {
            // main evaluation loop
            while (running) {
              r2::compile_fence();
            retry:
              // try one
              if (test_func(*qp, thread_id, R2_ASYNC_WAIT)) {
                statics[thread_id].increment(FLAGS_batch);
                std::cout << "value " << std::string((char *)local_mr.buf + 64)
                          << std::endl;
              } else {
                R2_YIELD;
                goto retry;
              }
              R2_YIELD;
            }
            R2_STOP();
            R2_RET; // a coroutine must use specific version to return
          });
        }
        R2_RET;
      });
      r.run();
#endif
      return 0;
      // end thread
    }));
  }
  for (auto i : threads)
    i->start();

  Reporter::report_thpt(statics, FLAGS_running_time);
  running = false;
#if 1
  for (auto i : threads) {
    // LOG(4) << "thread join with :"  << i->join();
    i->join();
    delete i;
  }
#endif
  delete[] buffer;
  return 0;
}
