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
    r2::util::FastRandom rand(0xdeadbeaf);

    using Tree = XTree<16, u64>;
    Tree t;

    std::vector<u64> check_keys;
    for (uint i = 0;i < key_scale; ++i) {
      auto key = rand.next();
      check_keys.push_back(key);
      t.insert(key, key + 73);
    }

    for (auto k : check_keys) {
      auto v = t.get(k);
      ASSERT_TRUE(v);
      ASSERT_EQ(v.value(), k + 73);
    }
    //
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
