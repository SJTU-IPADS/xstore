/// Try evaluating the optimal performance of XStore read/write
/// For each request, the benchmark will do as follows:
/// - 1. read sizeof(XNode) in XTree
/// - 2. read sizeof(payload) given the benchmark payload

#include <gflags/gflags.h>

#include "../xcomm/src/lib.hh"
#include "../xcomm/src/atomic_rw/rdma_rw_op.hh"

#include "./reporter.hh"
#include "../deps/r2/src/thread.hh"

#include "../deps/rlib/core/lib.hh"

DEFINE_int64(threads, 1, "num client thread to use");
DEFINE_int64(coros, 1, "num client coroutine used per threads");
DEFINE_int64(nic_idx, 0, "which RNIC to use");
DEFINE_int64(payload, 8, "value payload the client would fetch");
DEFINE_string(addr, "localhost:8888", "server address");

namespace bench {

using namespace xstore;
using namespace xcomm;
using namespace xstore::bench;
using namespace r2;
using namespace rdmaio;

using XThread = ::r2::Thread<usize>;
}

using namespace bench;

int main(int argc, char **argv) {

  gflags::ParseCommandLineFlags(&argc, &argv, true);

  std::vector<std::unique_ptr<XThread>> workers;

  std::vector<Statics> statics(FLAGS_threads);

  for (uint thread_id = 0; thread_id < FLAGS_threads; ++thread_id) {
    workers.push_back(
        std::move(std::make_unique<XThread>([&statics, thread_id]() -> usize {
          auto nic = RNic::create(RNicInfo::query_dev_names().at(FLAGS_nic_idx)).value();

          return 0;
        })));
  }

  for (auto &w : workers) {
    w->start();
  }

  Reporter::report_thpt(statics, 30);

  for (auto &w : workers) {
    w->join();
  }
}
