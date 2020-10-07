#include "rlib/rdma_ctrl.hpp"

#include "thread.hpp"
#include "utils/all.hpp"

#include "r2/src/timer.hpp"

#include "reporter.hpp"

#include <csignal>
#include <gflags/gflags.h>

#define ROCKSDB_BACKTRACE
//#include "rocksdbb/port/stack_trace.h"

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
#include "clients/mod.hpp"
#include "controler.hpp"

using namespace rdmaio;

namespace fstore {

__thread u64 data_transfered;

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
  // static RegionManager rm((char*)alloc_huge_page(FLAGS_memory, 2 * MB),
  // FLAGS_memory);
  return rm;
}

volatile bool running = false;
SC* sc = nullptr;

// workload identifiers
enum
{
  nop,
  null_workload,
  rpc_get,
  null_rdma,
  sc_rdma,
  sc_rdma_scan,
  rdma_round,
  rdma_span,
  rpc_scan,
  dynamic,
  dynamic2,
  dynamic_scan,
  tpcc,
  workload_shift,
  prod,
};

using workload_map_t = std::map<std::string, u32>;
static workload_map_t
get_workload()
{
  workload_map_t res = { { "nop", nop },
                         { "null", null_workload },
                         { "rpc_get", rpc_get },
                         { "null_rdma", null_rdma },
                         { "sc_rdma", sc_rdma },
                         { "sc_scan_rdma", sc_rdma_scan },
                         { "rdma_round", rdma_round },
                         { "rdma_span", rdma_span },
                         { "rpc_scan", rpc_scan },
                         { "dynamic", dynamic },
                         { "dynamic2", dynamic2 },
                         { "dynamic_scan", dynamic_scan },
                         { "tpcc", tpcc },
                         { "shift", workload_shift },
                         { "prod", prod } };
  return res;
};

} // namespace bench

} // namespace fstore

using namespace fstore::bench;

int
main(int argc, char** argv)
{

  //rocksdb::port::InstallStackTraceHandler();

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

  std::vector<Worker*> threads;
  PBarrier bar(FLAGS_threads + 1);

  switch (workloads_map[FLAGS_workloads]) {
    case null_workload:
      threads = NullClient::bootstrap_all(statics,
                                          { .mac_id = 0, .thread_id = 73 },
                                          FLAGS_server_host,
                                          FLAGS_server_port,
                                          bar,
                                          FLAGS_threads,
                                          FLAGS_id,
                                          FLAGS_concurrency);
      break;
    case rpc_get:
      threads = RPCGetClient::bootstrap_all(statics,
                                            { .mac_id = 0, .thread_id = 73 },
                                            FLAGS_server_host,
                                            FLAGS_server_port,
                                            bar,
                                            FLAGS_threads,
                                            FLAGS_id,
                                            FLAGS_concurrency);
      break;
    case null_rdma:
      threads = NullRDMAClient::bootstrap_all(statics,
                                              { .mac_id = 0, .thread_id = 73 },
                                              FLAGS_server_host,
                                              FLAGS_server_port,
                                              bar,
                                              FLAGS_threads,
                                              FLAGS_id,
                                              FLAGS_concurrency);
      break;
    case sc_rdma:
      threads = SCRDMAClient::bootstrap_all(statics,
                                            { .mac_id = 0, .thread_id = 73 },
                                            FLAGS_server_host,
                                            FLAGS_server_port,
                                            bar,
                                            FLAGS_threads,
                                            FLAGS_id,
                                            FLAGS_concurrency);
      break;
    case rdma_round:
      threads = RDMARoundtrips::bootstrap_all(statics,
                                              { .mac_id = 0, .thread_id = 73 },
                                              FLAGS_server_host,
                                              FLAGS_server_port,
                                              bar,
                                              FLAGS_threads,
                                              FLAGS_id,
                                              FLAGS_concurrency);
      break;
    case rdma_span:
      threads = RDMASpan::bootstrap_all(statics,
                                        { .mac_id = 0, .thread_id = 73 },
                                        FLAGS_server_host,
                                        FLAGS_server_port,
                                        bar,
                                        FLAGS_threads,
                                        FLAGS_id,
                                        FLAGS_concurrency);

      break;
    case rpc_scan:
      threads = RPCScanClient::bootstrap_all(statics,
                                             { .mac_id = 0, .thread_id = 73 },
                                             FLAGS_server_host,
                                             FLAGS_server_port,
                                             bar,
                                             FLAGS_threads,
                                             FLAGS_id,
                                             FLAGS_concurrency);
      break;
    case sc_rdma_scan:
      threads = SCRDMASClient::bootstrap_all(statics,
                                             { .mac_id = 0, .thread_id = 73 },
                                             FLAGS_server_host,
                                             FLAGS_server_port,
                                             bar,
                                             FLAGS_threads,
                                             FLAGS_id,
                                             FLAGS_concurrency);
      break;
    case dynamic:
      threads = D4::bootstrap(statics,
                                         { .mac_id = 0, .thread_id = 73 },
                                         FLAGS_server_host,
                                         FLAGS_server_port,
                                         bar,
                                         FLAGS_threads,
                                         FLAGS_id,
                                         FLAGS_concurrency);
      break;
    case dynamic2:
      threads = DE::bootstrap(statics,
                                           { .mac_id = 0, .thread_id = 73 },
                                           FLAGS_server_host,
                                           FLAGS_server_port,
                                           bar,
                                           FLAGS_threads,
                                           FLAGS_id,
                                           FLAGS_concurrency);

      break;

    case dynamic_scan:
      threads = DynamicScan::bootstrap(statics,
                                       { .mac_id = 0, .thread_id = 73 },
                                       FLAGS_server_host,
                                       FLAGS_server_port,
                                       bar,
                                       FLAGS_threads,
                                       FLAGS_id,
                                       FLAGS_concurrency);

      break;

    case tpcc:
      threads = TPCC::bootstrap(statics,
                                { .mac_id = 0, .thread_id = 73 },
                                FLAGS_server_host,
                                FLAGS_server_port,
                                bar,
                                FLAGS_threads,
                                FLAGS_id,
                                FLAGS_concurrency);

      break;

    case workload_shift:
      threads = Shift::bootstrap_all(statics,
                                     { .mac_id = 0, .thread_id = 73 },
                                     FLAGS_server_host,
                                     FLAGS_server_port,
                                     bar,
                                     FLAGS_threads,
                                     FLAGS_id,
                                     FLAGS_concurrency);
      break;

    case nop:
      threads = NopClient::bootstrap_all(statics,
                                         { .mac_id = 0, .thread_id = 73 },
                                         FLAGS_server_host,
                                         FLAGS_server_port,
                                         bar,
                                         FLAGS_threads,
                                         FLAGS_id,
                                         FLAGS_concurrency);
      break;
    case prod:
      threads = Product::bootstrap(statics,
                                   { .mac_id = 0, .thread_id = 73 },
                                   FLAGS_server_host,
                                   FLAGS_server_port,
                                   bar,
                                   FLAGS_threads,
                                   FLAGS_id,
                                   FLAGS_concurrency);
      break;
    default:
      assert(false); // not implemented
  };

  LOG(4) << "starting " << threads.size()
         << " threads, using workload: " << FLAGS_workloads;
  for (auto i : threads)
    i->start();
  LOG(4) << "all threads start done";

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
