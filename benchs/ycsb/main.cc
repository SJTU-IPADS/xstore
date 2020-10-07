#include "rlib/rdma_ctrl.hpp"

#include "thread.hpp"
#include "utils/all.hpp"

#include "r2/src/timer.hpp"

#include "../micro/clients/lib.hpp"
#include "mem_region.hpp"
#include "reporter.hpp"

#include <csignal>
#include <gflags/gflags.h>

#define ROCKSDB_BACKTRACE
//#include "rocksdbb/port/stack_trace.h"

#include "port/stack_trace.hpp"

using namespace fstore::utils;

DEFINE_int64(port, 8888, "My port used.");
DEFINE_string(host, "localhost", "My host used.");
DEFINE_uint64(concurrency, 1, "Number of coroutine per thread.");
DEFINE_uint64(running_time, 5, "Number of seconds for the client to run.");
DEFINE_uint64(threads, 1, "Number of client threads used.");
DEFINE_uint64(id, 1, "Client id, start from 1.");

DEFINE_bool(use_master, true, "Whether works in a standard alone mode");

#include "flags.hpp"

// Different client implementations
#include "controler.hpp"

#include "clients.hpp"

using namespace rdmaio;

namespace fstore {

namespace bench {

RdmaCtrl&
global_rdma_ctrl()
{
  static RdmaCtrl ctrl(FLAGS_port);
  return ctrl;
}

RegionManager&
global_memory_region()
{
  static RegionManager rm(FLAGS_memory);
  return rm;
}

volatile bool running = false;
SC* sc = nullptr;

// workload identifiers
enum
{
  ycsba,
  ycsbb,
  ycsbc
};

static void
segfault_handler(int sig)
{
  static std::mutex mutex;
  mutex.lock();
  LOG(4) << "segmentation fault!!!";
  StackTrace::print_stacktrace();
  exit(-1);
}

using workload_map_t = std::map<std::string, u32>;
static workload_map_t
get_workload()
{
  workload_map_t res = {
    { "ycsba", ycsba },
    { "ycsbb", ycsbb },
    { "ycsbc", ycsbc },
  };
  return res;
};

} // namespace bench

} // namespace fstore

using namespace fstore::bench;

int
main(int argc, char** argv)
{

  //rocksdb::port::InstallStackTraceHandler();
  signal(SIGSEGV, segfault_handler);

  gflags::ParseCommandLineFlags(&argc, &argv, true);
  auto& rm = global_memory_region();
  rm.make_rdma_heap();
  AllocatorMaster<>::init((char*)(rm.get_region("Heap").addr),
                          rm.get_region("Heap").size);

  std::vector<Statics> statics(FLAGS_threads);

  auto workloads_map = get_workload();
  if (workloads_map.find(FLAGS_workloads) == workloads_map.end()) {
    LOG(4) << "unsupported workload type: " << FLAGS_workloads << ";"
           << "all supported workloads: " << map_to_str(workloads_map);
    return -1;
  }

  PBarrier bar(FLAGS_threads +
               1); // threads test threads and one monitoring thread
  std::vector<Worker*> threads =
    Client::bootstrap(statics,
                      { .mac_id = 0, .thread_id = 73 },
                      FLAGS_server_host,
                      FLAGS_server_port,
                      bar,
                      FLAGS_threads,
                      FLAGS_id,
                      FLAGS_concurrency);

  LOG(4) << "starting threads, using workload: " << FLAGS_workloads;

  for (auto i : threads)
    i->start();

  if (!FLAGS_use_master) {
    LOG(4) << "wait for thread to be ready";
    running = true;
    r2::compile_fence();
    bar.wait();
    LOG(4) << "all threads started, now reporting.";

    Reporter::report_thpt(statics, FLAGS_running_time);
    running = false;
  } else {
    if (threads.size() > 0) {
      Controler::main_loop(statics, bar, FLAGS_id, controler_id);
    }
  }
  for (auto i : threads) {
    // LOG(4) << "thread join with :"  << i->join();
    i->join();
    delete i;
  }

  LOG(4) << "done";

  return 0;
}
