#pragma once

#include <gflags/gflags.h>

#include "utils/memory_util.hpp"

using namespace fstore::utils;

namespace fstore {

namespace bench {

DEFINE_string(server_host,"localhost","Server host used.");
DEFINE_int64(server_port,8888,"Server port used.");
DEFINE_int64(memory,2 * GB,"Client heap memory used.");
DEFINE_int64(epoch,10,"The time client will run.");

DEFINE_uint64(total_accts,10000,"Number of accounts of YCSB.");
DEFINE_uint64(seed,0xdeadbeaf,"Random seed used.");

DEFINE_bool(bind_core,true,"Bind thread to a specific core for better performance.");
DEFINE_bool(need_hash,false,"Whether YCSB need hash.");

DEFINE_string(workloads,"null", "Workload type used.");
DEFINE_string(eval_type,"rpc",  "Use RPC for workload test.");

DEFINE_int32(tree_depth,5,"The emulated tree depth");

DEFINE_int32(hybrid_ratio, 10,"The percentage of op to go with RDMA.");

}

} // fstore
