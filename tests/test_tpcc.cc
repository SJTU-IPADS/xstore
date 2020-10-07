#include <gtest/gtest.h>

#include "../src/data_sources/tpcc/schema.hpp"
#include "../src//data_sources/tpcc/stream.hpp"

using namespace fstore::sources::tpcc;

namespace test {

TEST(TPCC, Simple) {
}

TEST(TPCC,Stock) {
  uint64_t start_ware(12);
  uint64_t end_ware(90);

  StockGenerator generator(start_ware,end_ware);
  uint count = 0;
  for(generator.begin();generator.valid();generator.next()) {
    auto key = generator.key();
    ASSERT_GE(key.warehouse_id,start_ware);
    ASSERT_LE(key.warehouse_id,end_ware);
    ASSERT_GE(key.stock_id,0);
    ASSERT_LT(key.stock_id,num_items_per_warehosue);
    //LOG(4) << key.to_u64();
    count += 1;
  }
  ASSERT_GT(count,0);
  ASSERT_EQ(count,generator.total_stocks());
}

}
