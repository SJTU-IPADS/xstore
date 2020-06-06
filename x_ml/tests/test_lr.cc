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

  ASSERT_EQ(trained.w, base.w);
  ASSERT_EQ(trained.b, base.b);
}

}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
