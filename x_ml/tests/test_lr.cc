#include <gtest/gtest.h>

#include "../../xutils/marshal.hh"

#include "../src/lr/mod.hh"
#include "../src/lr/compact.hh"
#include "../src/lr/mlr.hh"

namespace test {

using namespace xstore::xml;
using namespace xstore::util;

TEST(XML, LR) {
  std::vector<u64> train_set;
  std::vector<u64> labels;

  double w = 73;
  double b = 0xdeadbeaf;

  LR base(w,b);

  for (uint i = 0;i < 120; ++ i) {
    train_set.push_back(i);
    labels.push_back(base.predict(i));
  }

  LR trained;
  ASSERT_NE(trained.w, base.w);
  trained.train(train_set, labels);

  ASSERT_NEAR(trained.w, base.w, 0.01);
  ASSERT_NEAR(trained.b, base.b, 0.01);
}

TEST(LR, OTHERS) {
  // test other LR
  std::vector<u64> train_set;
  std::vector<u64> labels;

  double w = 73;
  double b = 0xdeadbeaf;

  LR base(w, b);

  for (uint i = 0; i < 120; ++i) {
    train_set.push_back(i);
    labels.push_back(base.predict(i));
  }

  CompactLR clr;
  clr.train(train_set, labels);
  ASSERT_NEAR(static_cast<double>(clr.w), w,0.01);
  ASSERT_NEAR(static_cast<double>(clr.b), b, 100);

  using MLRT =  MLR<u32, CompactLR>;
  MLRT mlr;
  mlr.train(train_set,labels);
  ASSERT_NEAR(clr.w, mlr.lr.w, 0.001);
  ASSERT_NEAR(clr.b, mlr.lr.b, 0);
  //ASSERT_NEAR(static_cast<double>(mlr.lr.b), b, 1);
}

TEST(MLR, basic) {
  using MLRT = MLR<u32, CompactLR>;
  MLRT mlr;

  std::vector<u64> train_set = {0, 1, 2, 9, 10};
  std::vector<u64> labels = {0, 1, 2, 3, 4};

  mlr.train(train_set, labels);
  u32 base = 3;
  mlr.set_base(base);

  for (auto k : train_set) {
    ASSERT_LE(mlr.predict(k), base);
  }
}

TEST(LR, Serialize) {

  using MLRT = MLR<u32, CompactLR>;
  MLRT mlr;

  std::vector<u64> train_set = {0, 1, 2, 9, 10};
  std::vector<u64> labels = {0, 1, 2, 3, 4};

  mlr.train(train_set, labels);
  u32 base = 3;
  mlr.set_base(base);

  auto s = mlr.serialize();
  MLRT mlr2;
  ASSERT_NE(mlr2.base, mlr.base);
  mlr2.from_serialize(s);
  ASSERT_EQ(mlr2.base, mlr.base);
  ASSERT_EQ(mlr2.lr.w, mlr.lr.w);
  ASSERT_EQ(mlr2.lr.w, mlr.lr.w);

  // serialize LR
  {
    LR lr;
    lr.train(train_set, labels);
    auto s = lr.serialize();
    LR lr1;
    lr1.from_serialize(s);
    ASSERT_EQ(lr1.w, lr.w);
    ASSERT_EQ(lr1.b, lr.b);
  }
}

}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
