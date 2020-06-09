#include <gtest/gtest.h>

#include "../src/lr/mod.hh"
#include "../src/lr/compact.hh"
//#include "../src/nn.hh"
#include "../src/xmodel.hh"

#include "../../deps/kvs-workload/ycsb/hash.hh"

namespace test {

using namespace xstore::xml;

TEST(XML, XModel) {
  XSubModel<LR> x;
  //XSubModel<NN> x1(4);
  // TODO

  std::vector<u64> keys;
  std::vector<u64> labels;

  const int insert_num = 1000;

  for (uint i = 0;i < insert_num; ++i) {
    u64 key = ::kvs_workloads::ycsb::Hasher::hash(i);
    //u64 key = i;
    keys.push_back(key);
    labels.push_back(i);
  }

  std::sort(keys.begin(), keys.end());

  // start prepareing the training-set
  x.train(keys,labels);
  LOG(4) << "Err min:  " << x.err_min << "; max:" << x.err_max;

  XSubModel<CompactLR> x1;
  x1.train(keys, labels);
  LOG(4) << "Err min:  " << x1.err_min << "; max:" << x1.err_max;

  // sanity check correctness
  for (uint i = 0;i < keys.size(); ++i) {
    auto k = keys[i];
    auto range = x.get_predict_range(k);
    ASSERT_GE(i,std::get<0>(range));
    ASSERT_LE(i,std::get<1>(range));
  }

  /* fogort NN now, its too costly to train
  LOG(4) << x1.ml.net;
  LOG(4) << "------";
  x1.train(keys,labels);
  LOG(4) << x1.ml.net;
  LOG(4) << "Err NN min:  " << x1.err_min << "; max:" << x1.err_max;
  */
}
} // namespace test

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
