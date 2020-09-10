#include <gtest/gtest.h>

#include <algorithm>

#include "../src/xarray.hh"
#include "../src/xarray_iter.hh"

#include "../../deps/r2/src/random.hh"

// common impl of iter
#include "./test_iter.hh"

namespace test {

using namespace r2;
using namespace xstore;
using namespace xstore::xkv;

using Array = xstore::xkv::XArray<XKey,u64>;
r2::util::FastRandom rand(0xdeadbeaf);

auto gen_keys(const usize &num) -> std::vector<XKey> {
  std::vector<u64> all_keys;
  const usize num_keys = num;
  for (uint i = 0; i < num_keys; ++i) {
    all_keys.push_back(rand.next());
  }
  std::sort(all_keys.begin(), all_keys.end());
  std::vector<XKey> res;
  for (auto k : all_keys) {
    res.push_back(XKey(k));
  }
  return res;
}

TEST(Array, Basic) {
  const usize num_keys = 120;
  auto all_keys = gen_keys(num_keys);
  char *key_buf = new char[sizeof(XKey) * num_keys];
  char *val_buf = new char[sizeof(Array::VType) * num_keys];

  Array array(MemBlock(key_buf, sizeof(XKey) * num_keys),
              MemBlock(val_buf, sizeof(Array::VType) * num_keys));

  for (auto k : all_keys) {
    ASSERT_TRUE(array.insert(k, k.d));
  }
  ASSERT_FALSE(array.insert(XKey(std::numeric_limits<u64>::max()),0));

  for (uint i = 0;i < num_keys; ++i) {
    ASSERT_EQ(array.keys_at(i).value(), all_keys[i]);
    ASSERT_EQ(array.vals_at(i).value(), all_keys[i].d);
  }

  for (auto k : all_keys) {
    ASSERT_EQ(array.get(k).value(),k.d);
  }
}

TEST(XArray,Iter) {
  const usize num_keys = 120;
  auto all_keys = gen_keys(num_keys);
  char *key_buf = new char[sizeof(XKey) * num_keys];
  char *val_buf = new char[sizeof(Array::VType) * num_keys];

  Array array(MemBlock(key_buf, sizeof(XKey) * num_keys),
              MemBlock(val_buf, sizeof(Array::VType) * num_keys));

  for (auto k : all_keys) {
    ASSERT_TRUE(array.insert(k, k.d));
  }

  using AIter = ArrayIter<XKey,u64>;
  auto it = AIter::from(array);
  test_iter(all_keys, it);

  // check seek
  // because array's seek is quite different from others, e.g., B+Tree
  // so we use a customized function for testing
  for (auto k : all_keys) {
    auto it = AIter::from(array);
    it.seek(k, array);
    ASSERT_TRUE(it.has_next());
    ASSERT_EQ(it.cur_key(),k);
  }
}

} // namespace test

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
