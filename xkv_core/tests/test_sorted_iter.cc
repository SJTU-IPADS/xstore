#include <gtest/gtest.h>

#include <algorithm>

#include "../../deps/r2/src/random.hh"
#include "../src/xtree/mod.hh"
#include "../src/xtree/sorted_iter.hh"

#include "../src/xalloc.hh"

#include "./test_iter.hh"

namespace test {

using namespace xstore::xkv::xtree;
using namespace xstore::xkv;

TEST(Tree, Iter)
{
  const usize key_scale = 1000000;
  // const usize key_scale = 10;

  r2::util::FastRandom rand(0xdeadbeaf);

  using Tree = XTree<16, XKey, u64>;
  Tree t;

  std::vector<u64> check_keys;
  for (uint i = 0; i < key_scale; ++i) {
    auto key = rand.next();
    check_keys.push_back(key);
    t.insert(XKey(key), key + 73);
  }

  for (auto k : check_keys) {
    auto v = t.get(XKey(k));
    ASSERT_TRUE(v);
    ASSERT_EQ(v.value(), k + 73);
  }

  auto it = XTreeSIter<16, XKey, u64>::from(t);
  auto prev = XKey::min();

  usize count = 0;

  for (it.begin(); it.has_next(); it.next()) {
    auto key = it.cur_key();
    ASSERT_LE(prev, key);
    prev = key;
    count += 1;
  }
  ASSERT_EQ(count, check_keys.size());
}

} // namespace test
int
main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
