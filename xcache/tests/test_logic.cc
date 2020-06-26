#include <gtest/gtest.h>

#include "../src/logic_addr.hh"

namespace test {

using namespace xstore::xcache;
using namespace r2;

TEST(Logic, Basic) {
  auto original = LogicAddr::encode_logic_addr<128>(73, 64);
  ASSERT_EQ(LogicAddr::decode_off<128>(original), 64);
  ASSERT_EQ(LogicAddr::decode_logic_id<128>(original),73);
}

} // namespace test

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
