#include <gtest/gtest.h>

#include "../src/xtree/mod.hh"

namespace test {

using namespace xstore::xkv::xtree;

TEST(Tree,Basic) {

  using Tree = XTree<16, u64>;
  Tree t;

  /*
    simple tests
   */
  auto insert_cnt = 24;
  for (uint i = 0;i < insert_cnt;++i) {
    t.insert(i, i);
  }


  for (uint i = 0;i < insert_cnt;++i) {
    auto v = t.get(i);
    ASSERT_TRUE(v);
    ASSERT_EQ(v.value(), i);
  }
  /*
    simple tests passes
   */
}

TEST(Tree, allocation) {
  // test that there is no double alloc
}

}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
