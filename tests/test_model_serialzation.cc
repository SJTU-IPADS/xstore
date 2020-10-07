#include <gtest/gtest.h>

#include "common.hpp"
#include "model_config.hpp"
#include "model_serialize.hpp"
#include "mousika/learned_index.h"
#include "data_sources/ycsb/hash.hpp"

using namespace r2::util;
using namespace fstore;
using namespace fstore::sources::ycsb;
using namespace std;

namespace test {

TEST(Serialization,LR) {

  FastRandom rand(0xdeadbeaf);

  u64 sum = 0;
  for(uint i = 0;i < 1024;++i) {
    LinearRegression lr;
    lr.w         = rand.next_uniform();
    lr.bias      = rand.next_uniform() * 12;
    lr.min_error = rand.rand_number(0,1024);
    lr.max_error = rand.rand_number(1024,4096);

    string res = LinearRegression::serialize_hardcore(lr);
    auto lr2 = LinearRegression::deserialize_hardcore(res);
    ASSERT_EQ(lr.w,lr2.w);
    ASSERT_EQ(lr.bias,lr2.bias);
    ASSERT_EQ(lr.min_error,lr2.min_error);
    ASSERT_EQ(lr.max_error,lr2.max_error);

    sum += lr2.max_error;
  }
  LOG(2) << "sum: " << sum;
}

TEST(Serialization, LI) {
  // check that we can serialize
  const string template_config = "[[stages]]\n"
                                 "type = 'lr'\n"
                                 "parameter = 1\n"
                                 "\n"
                                 "[[stages]]\n"
                                 "type='lr'\n"
                                 "parameter = 1\n";

  stringstream ss(template_config);
  cpptoml::parser p{ss};
  RMIConfig config = ModelConfig::load_internal(p.parse());

  LearnedRangeIndexSingleKey<uint64_t,float> table(config);
  for(uint i = 0;i < 1024;++i)
    table.insert(Hasher::hash(i),0);
  table.finish_insert();

  LOG(2) << table.rmi.first_stage->models[0];
  LOG(2) << table.rmi.second_stage->models[0];

  vector<string> first,second;
  for(auto &m : table.rmi.first_stage->models) {
    first.push_back(LinearRegression::serialize_hardcore(m));
  }

  for(auto &m : table.rmi.second_stage->models) {
    second.push_back(LinearRegression::serialize_hardcore(m));
  }

  // ---------------------------------
  // add a stage of serialze transform
  DefaultAllocator alloc;
  auto res_bufs = ModelDescGenerator::generate_two_stage_desc<>(alloc,first,second);
  ASSERT_EQ(res_bufs.size(), 2);

  // ---------------------------------

  // now we use the serailized data to create the table
  //LearnedRangeIndexSingleKey<uint64_t,float> table2(first,second,table.sorted_array_size);
  LearnedRangeIndexSingleKey<uint64_t,float> table2(
      ModelDescGenerator::deserialize_one_stage(std::get<0>(res_bufs[0])),
      ModelDescGenerator::deserialize_one_stage(std::get<0>(res_bufs[1])),
      table.sorted_array_size);
  table2.rmi.key_n = table.rmi.key_n;

  LOG(2) << table2.rmi.first_stage->models[0];
  LOG(2) << table2.rmi.second_stage->models[0];

  for(uint i = 0;i < 1024;++i) {
    //LOG(4) << "test_trace: " << i;
    auto key = Hasher::hash(i);
    auto p0 = table.predict(key);
    auto p1 = table2.predict(key);
    /**
     * Serialization is tricky, especially for double.
     * The predictions can be slightly different due to serialization.
     */
    ASSERT_EQ(p0.start,p1.start);
    ASSERT_EQ(p0.pos,p1.pos);
    ASSERT_EQ(p0.end,p1.end);
  }

}

}
