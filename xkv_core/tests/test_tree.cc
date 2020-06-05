#include <gtest/gtest.h>

#include "../src/xtree/mod.hh"
#include "../../deps/r2/src/random.hh"

namespace test {

using namespace xstore::xkv::xtree;

TEST(Tree,Basic) {

  using Tree = XTree<16, u64>;
  Tree t;

  /*
    simple tests
   */
  auto insert_cnt = 150000;
  for (uint i = 0;i < insert_cnt;++i) {
    t.insert(i, i);
  }

  for (uint i = 0;i < insert_cnt;++i) {
    auto v = t.get(i);
    if (!v) {
      LOG(4) << "faild to find key: " << i << "; tree depth: " << t.depth;
    }
    ASSERT_TRUE(v);
    ASSERT_EQ(v.value(), i);
  }
  /*
    simple tests passes
   */
}

TEST(Tree, Stress) {

  const usize key_scale = 1000000;
  for (uint i = 0; i < 10; ++i) {
    std::vector<u64> check_keys;
    r2::util::FastRandom rand(0xdeadbeaf);
  }
}

TEST(Tree, allocation) {
  // test that there is no double alloc
}

}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
