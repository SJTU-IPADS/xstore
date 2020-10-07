#include <gtest/gtest.h>

#include "../deps/mousika/learned_index.h"
#include "../src/datastream/rocksdb_stream.hpp"

using namespace fstore::datastream;

namespace test {

TEST(Index, Rocks) {
#if 0
  RocksStream<uint64_t,uint64_t> db("./testdb");
  for(uint i = 0;i < 12;++i) {
    db.put(i,i);
  }
  db.close();
#endif
}

TEST(Index, Learned) {
#if 0
  RMIConfig rmi_config;
  RMIConfig::StageConfig first, second;

  // XD: which is first, and which is second means?
  // does this means that there are only 2 stages for the model?
  // pps: seems my guess is correct
  first.model_type = RMIConfig::StageConfig::LinearRegression;
  first.model_n = 1;

  second.model_n = 12;
  second.model_type = RMIConfig::StageConfig::LinearRegression;
  rmi_config.stage_configs.push_back(first);
  rmi_config.stage_configs.push_back(second);

  LearnedRangeIndexSingleKey<uint64_t,float> table(rmi_config);
  RocksStream<uint64_t,uint64_t> db("./testdb");
  auto it = db.get_iter();

  int count = 0;
  for(it->begin();it->valid();it->next()) {
    auto key = it->key();
    auto value = it->value();
    LOG(4) << key;
    table.insert(key,value);
    count++;
  }
  ASSERT_EQ(count,12);
  table.finish_insert();

  LOG(4) << "finished insert";

  it->begin();
  for(it->begin();it->valid();it->next()) {
    auto key = it->key();
    auto value = table.get(key);
    ASSERT_EQ(value,it->value());
  }
#endif
}

} // end namespace test
