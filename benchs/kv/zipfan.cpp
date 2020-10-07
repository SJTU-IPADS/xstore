#include "utils/zipfan.hpp"
#include "utils/data_map.hpp"

#include <vector>

using namespace fstore;
using namespace fstore::utils;

/**
 * Plot a zipfan distribution for easy to debug.
 */
int main() {

  const int total_keys = 64;
  ZipFanD zipfan(total_keys);

  std::vector<u64> counts(total_keys,0);

  for(uint i = 0;i < 20000;++i) {
    auto next = zipfan.next();
    counts[next] += 1;
  }

  DataMap<u64,u64> dist("zip");
  for(uint i = 0;i < counts.size();++i) {
    dist.insert(i,counts[i]);
  }
  FILE_WRITE("zip.py",std::ofstream::out) << dist.dump_as_np_data();
  return 0;
}
