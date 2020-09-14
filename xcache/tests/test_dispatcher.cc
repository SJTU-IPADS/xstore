#include <gtest/gtest.h>

#include <algorithm>

#include "../../xkv_core/src/xarray_iter.hh"

#include "../../x_ml/src/lr/mod.hh"
#include "../src/dispatcher.hh"

#include "../../deps/r2/src/random.hh"

namespace test {

using namespace xstore;
using namespace xstore::xcache;
using namespace xstore::xml;
using namespace r2;

r2::util::FastRandom rand(0xdeadbeaf);

TEST(XCache, Dispatcher) {
  using DT = Dispatcher<LR, XKey>;
  using A = XArray<XKey,u64>;

  char *key_buf = new char[sizeof(XKey) * 1200];
  char *val_buf = new char[sizeof(A::VType) * 1200];

  A array(MemBlock(key_buf, sizeof(XKey) * 1200),
          MemBlock(val_buf, sizeof(A::VType) * 1200));

  const usize d_num = 4;
  DT dt(d_num);
  DT dt1(d_num);

  std::vector<XKey> train_set;
  std::vector<u64> labels;

  std::vector<usize> dispatcher_counts(d_num, 0);
  std::vector<u64> all_keys;

  // 12 means all keys num
  for (uint i = 0; i < 1200; ++i) {
    all_keys.push_back(rand.next());
  }

  std::sort(all_keys.begin(), all_keys.end());

  for (uint i = 0; i < all_keys.size(); ++i) {
    array.insert(XKey(all_keys[i]),all_keys[i]);
    train_set.push_back(XKey(all_keys[i]));
    labels.push_back(i);
  }

  ASSERT_EQ(array.size, 1200);

  dt.model.train(train_set, labels);

  // dispatch
  for (uint i = 0; i < all_keys.size(); ++i) {
    auto d = dt.predict(XKey(all_keys[i]), all_keys.size());
    //LOG(4) << "predict :" << all_keys[i] << " val:" << d;

    ASSERT_GE(d, 0);
    ASSERT_TRUE(d < dispatcher_counts.size());
    dispatcher_counts[d] += 1;
  }

  // compute the average:
  usize sum = 0;
  for (uint i = 0; i < dt.dispatch_num; ++i) {
    sum += dispatcher_counts[i];
  }
  ASSERT_EQ(sum, all_keys.size());
  auto average = 10;
  for (uint i = 0; i < dt.dispatch_num; ++i) {
    //ASSERT_EQ(average, dispatcher_counts[i]);
    LOG(4) << dispatcher_counts[i];
  }

  // train w the array
  auto sz = dt1.default_train<ArrayIter<XKey,u64>>(array);
  ASSERT_EQ(sz, 1200);

  // check the train result
  ASSERT_EQ(dt1.model.w, dt.model.w);
}

} // namespace test

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
