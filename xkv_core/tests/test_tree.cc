#include <gtest/gtest.h>

#include "../src/xtree/mod.hh"

namespace test {

using namespace xstore::xkv::xtree;

TEST(Tree,Basic) {
  ASSERT_EQ(0,-1); // not implemented
}

}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
