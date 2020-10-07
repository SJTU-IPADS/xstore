#include <gtest/gtest.h>

#include "../src/msg/protocol.hpp"

using namespace r2;

namespace test {

TEST(ProtocolTest, Addr) {
  Addr address = { 12, 0 };
  ASSERT_EQ(address.to_u32(), 12);
}

} // end namespace test
