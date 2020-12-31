/// evaluate XStore's server insert performance

#include "../deps/r2/src/random.hh"
#include "../deps/r2/src/thread.hh"

#include "../xkv_core/src/xtree_con.hh"

#include "./reporter.hh"

#include "../lib.hh"

#include <gflags/gflags.h>

DEFINE_int64(threads, 1, "num insert thread to use");
DEFINE_int64(initial_load, 100000, "number of keys initial loaded");

namespace bench {

using namespace xstore::xkv;
using namespace xstore::bench;
using namespace xstore;
using namespace xstore::util;

using XThread = ::r2::Thread<usize>;

const usize kNodeMaxKeys = 16;

using Tree = CXTree<kNodeMaxKeys, XKey, u64>;
} // namespace bench

using namespace bench;

volatile bool running = true;

int main(int argc, char **argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  std::vector<std::unique_ptr<XThread>> workers;

  std::vector<Statics> statics(FLAGS_threads);

  Tree t;

  // init load the keys
  ::r2::util::FastRandom rand(0xdeadbeaf);
  for(uint i = 0;i < FLAGS_initial_load; ++i) {
    auto key = rand.next();
    t.insert(XKey(key),key);
  }

  for (uint thread_id = 0; thread_id < FLAGS_threads; ++thread_id) {
    workers.push_back(std::move(
        std::make_unique<XThread>([&statics, &t, thread_id]() -> usize {
          ::r2::util::FastRandom rand(0xdeadbeaf + (thread_id + 1) * 73);
          while (running) {
            auto key = rand.next();
            t.insert(XKey(key),key);
            statics[thread_id].increment();
          }
          return 0;
        })));
  }

  for (auto &w : workers) {
    w->start();
  }

  Reporter::report_thpt(statics, 30);

  r2::compile_fence();
  running = false;

  for (auto &w : workers) {
    w->join();
  }
  return 0;
}
