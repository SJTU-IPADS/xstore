#include <gtest/gtest.h>

#include "../src/lib.hh"

namespace test {

using namespace xstore::xkv;
using namespace r2;

TEST(XKV, FatPointer) {

  u64 *val = new u64;
  *val = 73;
  FatPointer p(val, 128);

  ASSERT_EQ(val, p.get_ptr<u64>());
  ASSERT_EQ(128,p.get_sz());
  ASSERT_EQ(*(p.get_ptr<u64>()),73);
}
} // namespace test

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
