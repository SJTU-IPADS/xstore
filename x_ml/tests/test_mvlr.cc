#include <gtest/gtest.h>

#include "../src/mv_lr.hh"
#include "./test_key.hh"

#include "../src/lr/mod.hh"

namespace test {

using namespace xstore;
using namespace xml;

using MLR = MvLR<4, TestKey>;

TEST(MVLR, Basic)
{
  std::vector<TestKey> train_set;
  std::vector<u64> labels;

  train_set.push_back(TestKey(1, 4, 12, 1));
  train_set.push_back(TestKey(1, 4, 12, 2));

  for (uint i = 0; i < train_set.size(); ++i) {
    labels.push_back(i);
  }

  MLR lr;
  lr.train(train_set, labels);
  LOG(4) << "after trained: " << lr;

  for (uint i = 0; i < train_set.size(); ++i) {
    LOG(4) << "predict: " << train_set[i] << " " << lr.predict(train_set[i])
           << "; label: " << labels[i];
  }
}

// MvLR should behave has LR when D =1
TEST(MVLR, LR)
{
  std::vector<XKey> train_set;
  std::vector<u64> train_label;
  using FLR = MvLR<1, XKey>; // faked LR

  double w = 73;
  double b = 0xdeadbeaf;

  LR<XKey> base(w, b);

  for (uint i = 0; i < 120; ++i) {
    train_set.push_back(XKey(i));
    train_label.push_back(base.predict(XKey(i)));
  }

  FLR lr;
  lr.train(train_set, train_label);
  LOG(4) << "faked LR: " << lr;
}

TEST(MVLR, Serialize)
{
  MLR lr0({ 1, 2, 3, 4, 5 });
  LOG(4) << lr0;

  MLR lr1;
  lr1.from_serialize(lr0.serialize());
  LOG(4) << lr1;
  ASSERT_EQ(lr0.w_vec[0], lr1.w_vec[0]);
}
}

int
main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}