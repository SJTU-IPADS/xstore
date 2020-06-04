#include <gtest/gtest.h>

#include <vector>

#include "../src/xtree/xnode.hh"

#include "../../deps/r2/src/random.hh"

namespace test {

using namespace xstore::xkv::xtree;

TEST(XNode, offset) {
  using Node = XNodeKeys<16>;

  Node n;

  std::vector<u64> check_keys;
  r2::util::FastRandom rand(0xdeadbeaf);

  for (uint i = 0;i < 16; ++i) {
    u64 key = rand.next();
    ASSERT_TRUE(n.add_key(key));
    check_keys.push_back(key);
  }

  // now check
  for (uint i = 0;i < check_keys.size();++i) {
    auto key = check_keys[i];
    u64 *node_k_ptr = reinterpret_cast<u64 *>(
        reinterpret_cast<char *>(&n) + n.key_offset(i));
    ASSERT_EQ(*node_k_ptr, key);
  }
}

TEST(XNode, Nodeoffset) {
  using Node = XNode<16, u64>;

  Node n;

  std::vector<u64> check_keys;
  r2::util::FastRandom rand(0xdeadbeaf);

  for (uint i = 0; i < 16; ++i) {
    u64 key = rand.next();
    ASSERT_TRUE(n.add_key(key));
    check_keys.push_back(key);
  }

  // now check
  for (uint i = 0; i < check_keys.size(); ++i) {
    auto key = check_keys[i];
    u64 *node_k_ptr =
        reinterpret_cast<u64 *>(reinterpret_cast<char *>(&n) + n.key_offset(i));
    ASSERT_EQ(*node_k_ptr, key);
  }
}
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
