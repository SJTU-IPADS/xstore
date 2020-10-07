#include <gtest/gtest.h>

#include "../src/data_sources/tpcc/schema.hpp"
#include "../src//data_sources/tpcc/stream.hpp"
#include "../src/data_sources/tpcc/workloads.hpp"

#include <unordered_map>

using namespace fstore;
using namespace fstore::sources;
using namespace fstore::sources::tpcc;

namespace test {

TEST(TPCC, StockWorkload) {

  uint64_t start_ware(12);
  uint64_t end_ware(32);

  std::unordered_map<u64,char> all_keys;

  StockGenerator generator(start_ware,end_ware);
  for(generator.begin();generator.valid();generator.next()) {
    all_keys.insert(std::make_pair(generator.key().to_u64(),0));
  }

  u64 sum = 0;
  //StockWorkload sw(start_ware,end_ware);
  BaseWorkload<StockWorkload> *wl = new StockWorkload(start_ware,end_ware);

  for(uint i = 0;i < 10000;++i) {
    auto key = wl->next_key();
    ASSERT_NE(all_keys.find(key),all_keys.end());
    sum += key;
  }

  delete wl;
  LOG(3) << "get sum: " << sum;
}

} // end namespace test
