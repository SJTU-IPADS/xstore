#include <gtest/gtest.h>

#include "../src/nmsg/net_naming.hpp"

using namespace r2;

namespace test
{
TEST(Netnaming, Basic)
{
    AddrIDTester::test_encoding();
}

} // namespace test
