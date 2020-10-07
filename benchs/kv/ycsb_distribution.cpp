#include "common.hpp"
#include "data_sources/ycsb/stream.hpp"
#include "utils/data_map.hpp"

#include <gflags/gflags.h>

#include <vector>
#include <algorithm>

DEFINE_int32(num,10000,"Total number of records for YCSB data sets.");

using namespace fstore;
using namespace fstore::sources::ycsb;
using namespace fstore::utils;

/*!
  Dump a sample YCSB datasets.
*/
int main(int argc,char **argv) {

  LOG(4) << "use " << FLAGS_num << " ycsb generator.";
  YCSBHashGenereator generator(0,FLAGS_num);

  std::vector<u64> all_keys;
  for(generator.begin();generator.valid();generator.next()) {
    all_keys.push_back(generator.key());
  }

  std::sort(all_keys.begin(),all_keys.end());

  DataMap<u64,u64> data("ycsb");
  for(uint i = 0;i < all_keys.size();++i)
    data.insert(all_keys[i],i);

  FILE_WRITE("ycsb.py",std::ofstream::out) << data.dump_as_np_data();

  return 0;
}
