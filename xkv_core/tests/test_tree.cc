#include <gtest/gtest.h>

#include <algorithm>

#include "../../deps/r2/src/random.hh"
#include "../src/xtree/iter.hh"
#include "../src/xtree/mod.hh"

#include "../src/xalloc.hh"

#include "./test_iter.hh"

namespace test {

using namespace xstore::xkv::xtree;
using namespace xstore::xkv;

TEST(Tree, Basic) {

  using Tree = XTree<16, XKey, u64>;
  Tree t;

  /*
    simple tests
   */
  auto insert_cnt = 150000;
  for (uint i = 0; i < insert_cnt; ++i) {
    t.insert(XKey(i), i);
  }

  LOG(4) << "insert done";

  for (uint i = 0; i < insert_cnt; ++i) {
    auto v = t.get(XKey(i));
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

r2::util::FastRandom rand(0xdeadbeaf);

TEST(Tree, Stress) {

  const usize key_scale = 1000000;

  for (uint i = 0; i < 10; ++i) {

    using Tree = XTree<16,XKey, u64>;
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
    //
  }
}

TEST(Tree, allocation) {
  // test that there is no double alloc
  using Tree = XTree<16, XKey, u64>;
  Tree t;

  auto total_keys = 1000000;

  auto total_sz = total_keys * sizeof(Tree::Leaf) / 8; // 8 = 16 / 2
  auto alloc = XAlloc<sizeof(Tree::Leaf)>(new char[total_sz], total_sz);

  t.init_pre_alloced_leaf(alloc);

  int split_num = 0;
  for (int i = 0; i < total_keys; ++i) {
    auto ret = t.insert_w_alloc(XKey(i), i, alloc);
    if (ret) {
      split_num += 1;
    }
  }
  ASSERT_EQ(alloc.cur_alloc_num, split_num + 1);
}

TEST(Tree, Iter) {
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

  std::sort(check_keys.begin(), check_keys.end());
  // using the iterator

  auto it = XTreeIter<16,XKey, u64>::from(t);
  usize counter = 0;

  LOG(4) << "f key: " << it.cur_key() << " " << check_keys[0]
         << "; invalid k: " << kInvalidKey;
#if 0 // the iterator semantic has changed, so the following test are legacy
  std::vector<u64> temp;
  std::vector<u64> batch;
  u64 cur_n = 0;
  for (it.seek(0, t); it.has_next(); it.next()) {

    if (cur_n != it.opaque_val()) {
      cur_n = it.opaque_val();
      if (!batch.empty()) {
        std::sort(batch.begin(), batch.end());
        for (uint i = 0; i < batch.size(); ++i) {
          temp.push_back(batch[i]);
          ASSERT_EQ(temp[temp.size() - 1], check_keys[temp.size() - 1]);
        }
        batch.clear();
      }
    }

    batch.push_back(it.cur_key());
    counter += 1;
  }

  if (!batch.empty()) {
    std::sort(batch.begin(), batch.end());
    for (uint i = 0; i < batch.size(); ++i) {
      temp.push_back(batch[i]);
      ASSERT_EQ(temp[temp.size() - 1], check_keys[temp.size() - 1]);
    }
  }
  ASSERT_EQ(counter, temp.size());
  // iter should find all keys
  ASSERT_EQ(counter, check_keys.size());
#endif
}

auto gen_keys(const usize &num) -> std::vector<u64> {
  std::vector<u64> all_keys;
  const usize num_keys = num;
  for (uint i = 0; i < num_keys; ++i) {
    all_keys.push_back(rand.next());
  }
  std::sort(all_keys.begin(), all_keys.end());
  return all_keys;
}

TEST(Tree, Iter1) {
  const usize num_keys = 120;
  auto all_keys = gen_keys(num_keys);

  using Tree = XTree<16, XKey, u64>;
  Tree t;

  for (auto k : all_keys) {
    t.insert(XKey(k),k);
  }

  auto it = XTreeIter<16,XKey,u64>::from(t);
  std::vector<XKey> all_keys_w;
  for (auto k : all_keys) {
    all_keys_w.push_back(XKey(k));
  }
  test_iter(all_keys_w, it);
}
} // namespace test

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
