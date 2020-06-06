#include <gtest/gtest.h>

#include "../src/lr/mod.hh"
#include "../src/xmodel.hh"

    namespace test {

using namespace xstore::xml;

TEST(XML, XModel) {
  XSubModel<LR> x;
  // TODO
}
} // namespace test

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
