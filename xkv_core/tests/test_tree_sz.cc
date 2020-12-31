#include <gtest/gtest.h>

#include "../src/xtree/mod.hh"

namespace test {

using namespace xstore;
using namespace xstore::xkv::xtree;
using namespace xstore::xkv;

TEST(Tree, Basic) {
  using Tree = XTree<16, XKey, u64>;
  Tree t;
  t.insert(XKey(0), 0);

  ASSERT_EQ(t.sz_inner(), 0);

  for (uint i = 1; i <= 32;++i) {
    t.insert(XKey(i), 0);
  }
  ASSERT_EQ(t.sz_inner(), sizeof(Tree::Inner));
}
} // namespace test

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
