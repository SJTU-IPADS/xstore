#include <gtest/gtest.h>

#include "../../xutils/marshal.hh"

#include "../src/lr/mod.hh"
#include "../src/lr/compact.hh"

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
}

TEST(LR, Serialize) {
  LR test(0.73, 0xdeadbeaf);
  auto res = MarshalT<LR>::serialize(test);
}

}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
