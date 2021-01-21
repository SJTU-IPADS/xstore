#include <gtest/gtest.h>

#include <vector>

#include "../../lib.hh"

#include "../src/xtree/xnode.hh"

#include "../../deps/r2/src/random.hh"

namespace test {

using namespace xstore;
using namespace xstore::xkv::xtree;

TEST(XNode, offset)
{
  using Node = XNodeKeys<16, XKey>;

  Node n;

  std::vector<u64> check_keys;
  r2::util::FastRandom rand(0xdeadbeaf);

  for (uint i = 0; i < 16; ++i) {
    u64 key = rand.next();
    ASSERT_TRUE(n.add_key(XKey(key)));
    check_keys.push_back(key);
  }

  // now check
  for (uint i = 0; i < check_keys.size(); ++i) {
    auto key = check_keys[i];
    XKey* node_k_ptr =
      reinterpret_cast<XKey*>(reinterpret_cast<char*>(&n) + n.key_offset(i));
    ASSERT_EQ(node_k_ptr->d, key);
  }
}

TEST(XNode, Nodeoffset)
{

  using Node = XNode<16, XKey, u64>;
  Node n;

  std::vector<u64> check_keys;
  r2::util::FastRandom rand(0xdeadbeaf);
#if XNODE_KEYS_ATOMIC
  WrappedType<Node::NodeK>* w = reinterpret_cast<WrappedType<Node::NodeK>*>(
    reinterpret_cast<char*>(&n) + n.keys_start_offset());
  Node::NodeK* nks = w->get_payload_ptr();

  for (uint i = 0; i < 16; ++i) {
    u64 key = rand.next();
    ASSERT_TRUE(nks->add_key(XKey(key)));
    check_keys.push_back(key);
  }

  // now check
  for (uint i = 0; i < check_keys.size(); ++i) {
    auto key = check_keys[i];
    auto node_k_ptr = reinterpret_cast<XKey*>(reinterpret_cast<char*>(nks) +
                                              nks->key_offset(i));
    ASSERT_EQ(node_k_ptr->d, key);
    ASSERT_EQ(n.get_key(i).d, key);
  }

  // check search
  for (auto k : check_keys) {
    ASSERT_TRUE(n.keys_ptr()->search(XKey(k)));
  }
#endif
}

TEST(XNode, Insert)
{
  using Node = XNode<16, XKey, u64>;
  Node n;

  std::vector<u64> check_keys;
  r2::util::FastRandom rand(0xdeadbeaf);

  // first check that we have enough space for the insertions
  for (uint i = 0; i < 16; ++i) {
    u64 key = rand.next();
    n.insert(XKey(key), key, nullptr);
    check_keys.push_back(key);
  }

  for (auto k : check_keys) {
    auto idx = n.keys_ptr()->search(XKey(k));
    ASSERT_TRUE(idx);
    ASSERT_EQ(k, (n.values.inplace[idx.value()]));
  }

  // check 1000 times
  for (uint t = 0; t < 1000; ++t) {
    check_keys.clear();
    Node n;
    Node n1;
    Node n2;

    auto prev_incar = n.get_incarnation();

    Option<u64> pivot_key = {};

    auto insert_num = rand.next() % 24 + 1; // slightly smaller than 2 * N

    for (uint i = 0; i < insert_num; ++i) {
      auto key = rand.next();
      check_keys.push_back(key);
      if (!pivot_key) {
        // n1 has not been inserted
        if (n.insert(XKey(key), key, &n1)) {
          // split
          pivot_key = n1.get_key(0).d;
          ASSERT_NE(n1.get_key(0), XKey(kInvalidKey));
          // LOG(4) << "split done for key: "<< key;
        }
      } else {
        if (key >= pivot_key.value()) {
          LOG(0) << "insert at pivot: " << key
                 << " ; num keys: " << n1.num_keys()
                 << "; n0: " << n.num_keys();
          ASSERT_FALSE(n1.insert(XKey(key), key, nullptr)); // should not split!
        } else {
          ASSERT_FALSE(n.insert(XKey(key), key, nullptr));
        }
      }
      // end insert all keys
    }

    // then check
    for (auto k : check_keys) {
      ASSERT_TRUE(n.search(XKey(k)) || n1.search(XKey(k)));
      ASSERT_TRUE((!(n.search(XKey(k)))) ||
                  (!(n1.search(XKey(k))))); // should not exist simuatously
      ASSERT_EQ(insert_num, n.num_keys() + n1.num_keys());
      auto idx = n.search(XKey(k));
      if (!idx) {
        idx = n1.search(XKey(k));
        ASSERT_EQ(k, n1.get_value(idx.value()).value());
      } else {
        ASSERT_TRUE(idx);
        ASSERT_EQ(k, n.get_value(idx.value()).value());
      }
    }

    // finally check the incarnation
    if (check_keys.size() > 16) {
      ASSERT_NE(prev_incar, n.get_incarnation());
    }
    LOG(0) << "done one";
  }
}

} // namespace test

int
main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
