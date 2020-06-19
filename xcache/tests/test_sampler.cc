#include <gtest/gtest.h>

#include "../src/samplers/step_sampler.hh"

namespace test {

using namespace xstore::xcache;

TEST(Sampler, step) {
  std::vector<KeyType> t_set;
  std::vector<u64> labels;

  StepSampler s(1);

  int num = 1024;

  for (uint i = 0;i < num; ++i) {
    s.add_to(i,i,t_set, labels);
  }

  ASSERT_EQ(t_set.size(), labels.size());
  ASSERT_EQ(t_set.size(), num);

  // step two
  t_set.clear();
  labels.clear();

  StepSampler s1(2);

  for (uint i = 0; i < num; ++i) {
    s1.add_to(i, i, t_set, labels);
  }

  ASSERT_EQ(t_set.size(), labels.size());
  ASSERT_EQ(t_set.size(), num / 2);

  // step four
  t_set.clear();
  labels.clear();

  StepSampler s2(4);

  for (uint i = 0; i < num; ++i) {
    s2.add_to(i, i, t_set, labels);
  }

  ASSERT_EQ(t_set.size(), labels.size());
  ASSERT_EQ(t_set.size(), num / 4);
}

}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
