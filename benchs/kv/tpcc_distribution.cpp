#include "common.hpp"
#include "data_sources/tpcc/stream.hpp"
#include "utils/data_map.hpp"

#include <gflags/gflags.h>

#include <vector>
#include <algorithm>

DEFINE_int32(num_ware,10,"Total number of records for YCSB data sets.");

using namespace fstore;
using namespace fstore::sources::tpcc;
using namespace fstore::utils;

/*!
  Dump a sample TPC-C stock datasets.
*/
int main(int argc,char **argv) {

  LOG(4) << "use " << FLAGS_num_ware << " TPC-C stock generator.";
  StockGenerator generator(1,1 + FLAGS_num_ware);

  std::vector<u64> all_keys;
  for(generator.begin();generator.valid();generator.next()) {
    all_keys.push_back(generator.key().to_u64());
  }

  std::sort(all_keys.begin(),all_keys.end());

  DataMap<u64,u64> data("tpcc_stock");
  for(uint i = 0;i < all_keys.size();++i)
    data.insert(all_keys[i],i);

  FILE_WRITE("tpcc.py",std::ofstream::out) << data.dump_as_np_data();

  return 0;
}
