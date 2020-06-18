#include <gtest/gtest.h>

#include "../src/xarray.hh"

#include "../../deps/r2/src/random.hh"

namespace test {

using namespace r2;

using Array = xstore::xkv::XArray<u64>;
r2::util::FastRandom rand(0xdeadbeaf);

TEST(Array, Basic) {
  std::vector<u64> all_keys;
  const usize num_keys = 120;
  for (uint i = 0; i < num_keys; ++i) {
    all_keys.push_back(rand.next());
  }

  std::sort(all_keys.begin(), all_keys.end());

  char *key_buf = new char[sizeof(u64) * num_keys];
  char *val_buf = new char[sizeof(Array::VType) * num_keys];

  Array array(MemBlock(key_buf, sizeof(u64) * num_keys),
              MemBlock(val_buf, sizeof(Array::VType) * num_keys));

  for (auto k : all_keys) {
    ASSERT_TRUE(array.insert(k, k));
  }
  ASSERT_FALSE(array.insert(std::numeric_limits<u64>::max(),0));

  for (uint i = 0;i < num_keys; ++i) {
    ASSERT_EQ(array.keys_at(i).value(), all_keys[i]);
    ASSERT_EQ(array.vals_at(i).value(), all_keys[i]);
  }

  for (auto k : all_keys) {
    ASSERT_EQ(array.get(k).value(),k);
  }
}

} // namespace test

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
