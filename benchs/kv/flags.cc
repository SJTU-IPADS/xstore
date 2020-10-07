#include <gflags/gflags.h>

namespace kv {

DEFINE_int64(threads, 1, "Number of test thread used.");
DEFINE_int64(page_sz_m, 10240, "Number of memory used for data tuples, in MB.");
DEFINE_string(model_config,
              "learned.toml",
              "The learned index model used for smart cache.");

DEFINE_string(workload_type, "ycsbh", "Which workload to serve.");
DEFINE_int64(seed, 0xdeadbeaf, "Init random seed used.");
DEFINE_int64(batch_size, 1000, "Number of batched item to sum.");

DEFINE_int64(running_time, 10, "The seconds of running benchmarks.");

/*!
  Benchmarks parameters for TPC-C
 */
DEFINE_int64(start_warehouse, 1, "The start id of the loading warehouse.");
DEFINE_int64(end_warehouse, 2, "The end id of the loading warehouse.");

/*!
  Benchmarks parameters for YCSB
*/
DEFINE_int64(num, 10000000, "Num of accounts loaded for YCSB.");

/*!
  Benchmarks parameters for other databases
 */
DEFINE_string(txt_name,
              "key_data/osm-uni-10000000.txt",
              "The txt file used to init the database");

}
