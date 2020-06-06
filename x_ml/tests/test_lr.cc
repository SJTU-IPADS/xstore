#include <gtest/gtest.h>

#include "../src/lr/mod.hh"

namespace test {

using namespace xstore::xml;

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

}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
