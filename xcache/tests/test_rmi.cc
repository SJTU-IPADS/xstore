#include <gtest/gtest.h>

#include "../../x_ml/src/lr/mod.hh"
#include "../../xkv_core/src/xarray_iter.hh"

#include "../src/submodel_trainer.hh"

#include "../../deps/r2/src/random.hh"

namespace test {

using namespace xstore::xcache;
using namespace xstore::xml;
using namespace r2;

r2::util::FastRandom rand(0xdeadbeaf);

using A = XArray<u64>;

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
    array.insert(k, k);
  }

  // init
  ASSERT_FALSE(all_keys.empty());
  XMLTrainer trainer;
  trainer.update_key(all_keys[0]);
  trainer.update_key(all_keys[all_keys.size() - 1]);

  // train
  DefaultSample s;
  auto model = trainer.train<ArrayIter<u64>, DefaultSample, LR>(array, s);
  LOG(4) << "error: " << model.total_error();

  for (auto k : all_keys) {
    // check whether we can find the key using the model
    auto predict = model.get_point_predict(k);

    bool found = false;
    auto p_range = model.get_predict_range(k);
    for (int i = std::get<0>(p_range); i <= std::get<1>(p_range); ++i) {
      auto res = array.keys_at(i);
      if (res && res.value() == k) {
        found = true;
        break;
      }
    }
    ASSERT_TRUE(found);
  }
}

} // namespace test

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
