#include <gtest/gtest.h>

#include "../../x_ml/src/lr/mod.hh"
#include "../../x_ml/src/lr/compact.hh"
#include "../../xkv_core/src/xarray_iter.hh"

#include "../src/rmi_2.hh"
#include "../src/submodel_trainer.hh"

#include "../../deps/r2/src/random.hh"

namespace test {

using namespace xstore;
using namespace xstore::xcache;
using namespace xstore::xml;
using namespace r2;

r2::util::FastRandom rand(0xdeadbeaf);

using A = XArray<XKey,u64>;

// test the correctness of submodel impl
TEST(RMI, Sub_array) {

  std::vector<u64> all_keys;
  int num_keys = 2048;

  // 12 means all keys num
  for (uint i = 0; i < num_keys; ++i) {
    all_keys.push_back(rand.next());
  }
  std::sort(all_keys.begin(), all_keys.end());

  A array(num_keys);

  for (auto k : all_keys) {
    array.insert(XKey(k), k);
  }

  // init
  ASSERT_FALSE(all_keys.empty());
  XMLTrainer<XKey> trainer;
  trainer.update_key(XKey(all_keys[0]));
  trainer.update_key(XKey(all_keys[all_keys.size() - 1]));

  // train
  DefaultSample<XKey> s;
  auto model = trainer.train<ArrayIter<XKey, u64>, DefaultSample, CompactLR>(array, s);
  LOG(4) << "error: " << model->total_error()
         << " for training: " << all_keys.size() << " tuples using a single LR";

  for (auto k : all_keys) {
    // check whether we can find the key using the model
    auto predict = model->get_point_predict(XKey(k));

    bool found = false;
    auto p_range = model->get_predict_range(XKey(k));
    for (int i = std::get<0>(p_range); i <= std::get<1>(p_range); ++i) {
      auto res = array.keys_at(i);
      if (res && res.value() == XKey(k)) {
        found = true;
        break;
      }
    }
    ASSERT_TRUE(found);
  }
}

TEST(RMI, Full) {
  std::vector<u64> all_keys;
  int num_keys = 2048;

  // 12 means all keys num
  for (uint i = 0; i < num_keys; ++i) {
    all_keys.push_back(rand.next());
    //all_keys.push_back(i);
  }
  std::sort(all_keys.begin(), all_keys.end());

  A array(num_keys);

  for (auto k : all_keys) {
    array.insert(XKey(k), k);
  }

  // init
  ASSERT_FALSE(all_keys.empty());

  const usize num_rmi = 12;
  LocalTwoRMI<LR, LR, XKey> rmi_idx(num_rmi);
  rmi_idx.default_train_first<ArrayIter<XKey,u64>>(array);

  DefaultSample<XKey> s;
  Statics statics;
  rmi_idx.train_second_models<ArrayIter<XKey,u64>,DefaultSample>(array,s,statics);

  for (uint i = 0; i < num_rmi; ++i) {
    LOG(4) << "model #" << i
           << " responsible for keys: " << rmi_idx.second_layer[i]->max
           << "; error: " << rmi_idx.second_layer[i]->total_error();
  }

  // testing
  for (auto k : all_keys) {

    bool found = false;
    auto p_range = rmi_idx.get_predict_range(XKey(k));
    for (int i = std::get<0>(p_range); i <= std::get<1>(p_range); ++i) {
      auto res = array.keys_at(i);
      if (res && res.value() == XKey(k)) {
        found = true;
        break;
      }
    }
    if (!found) {
      LOG(4) << "failed to find key: " << k
             << "; range:" << std::get<0>(p_range) << ":"
             << std::get<1>(p_range);
    }
    ASSERT_TRUE(found);
  }
}

} // namespace test

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
