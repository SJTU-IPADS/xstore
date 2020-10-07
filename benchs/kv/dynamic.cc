#include "hybrid_store.hpp"
#include "learned_analysis.hpp"
#include "model_config.hpp"
#include "sc_statics.hpp"
#include "smart_cache.hpp"
#include "thread.hpp"

#include "utils/all.hpp"

#include "flags.hpp"
#include "reporter.hpp"

#include "data_sources/tpcc/workloads.hpp"
#include "data_sources/ycsb/workloads.hpp"

#include "deps/r2/src/thread.hpp"
#include "deps/r2/src/timer.hpp"

#include "../server/internal/learner.hpp"
#include "../server/internal/tables.hpp"
#include "../server/loaders/loader.hpp"

#include <tuple>

using namespace fstore;
using namespace fstore::utils;
using namespace fstore::store;
using namespace fstore::server;

using namespace kv;
using namespace r2;

Tables global_table;

int
main(int argc, char** argv)
{

  gflags::ParseCommandLineFlags(&argc, &argv, true);

  u64 memory_sz = FLAGS_page_sz_m * MB;
  char* buf = new char[memory_sz];
  BlockPager<::kv::Leaf>::init(buf, memory_sz);
  LOG(4) << "store uses " << format_value(FLAGS_page_sz_m) << " MB.";

  Timer timer;
  ::kv::Tree t;
  auto id = global_table.register_table("ycsbh", FLAGS_model_config);
  ASSERT(id == 0);

  /// spawn a background thread to scan the tree for training
  ::r2::Thread<double> train_thread([&] {
    for (uint i = 0; i < 10; ++i) {
      Timer tt;
      //      Learner::clear_model(global_table.get_table(0),
      //      FLAGS_model_config);
      Learner::train(global_table.get_table(0));
      LOG(3) << "Trained epoch [" << i
             << "] trained : " << format_value(tt.passed_sec(), 1) << "seconds";
      sleep(1);
    }
    return 0;
  });
  train_thread.start();

  uint num_tuples(0);
  {
    num_tuples = YCSBLoader::populate_hash(
      global_table.get_table(0).data, FLAGS_num, 0xdeadbeaf);
  }

  LOG(4) << "load " << num_tuples << " using "
         << format_value(timer.passed_msec(), 1) << "useconds";
  train_thread.join();

  return 0;
}
