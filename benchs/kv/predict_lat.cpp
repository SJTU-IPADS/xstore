#include "flags.hpp"
#include "loader.hpp"

#include "model_config.hpp"
#include "utils/all.hpp"

namespace kv {

}

using namespace kv;
using namespace fstore;
using namespace fstore::utils;

int main(int argc,char **argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  // We use B+tree as the default data structures.
  u64 memory_sz = FLAGS_page_sz_m * MB;
  char *buf = new char[memory_sz];
  BlockPager<Leaf>::init(buf,memory_sz);

  // load the database
  Tree t;     uint num_tuples(0);
  {
    if(FLAGS_workload_type == "tpcc_stock") {
      num_tuples = DataLoader::populate_tpcc_stock_db(t,FLAGS_start_warehouse,FLAGS_end_warehouse,FLAGS_seed);
    } else if (FLAGS_workload_type == "ycsb" ) {
      num_tuples = DataLoader::populate_ycsb_hash_db(t, FLAGS_num, FLAGS_seed);
    }
  }
  LOG(4) << "total " << fstore::utils::format_value(num_tuples)
         << " loaded, use database: " << FLAGS_workload_type;

  auto model_config = ModelConfig::load(FLAGS_model_config);
  LOG(4) << model_config;

  return 0;
}
