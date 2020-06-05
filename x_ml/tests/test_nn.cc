#pragma once

#include "tiny_dnn/tiny_dnn.h"

using namespace tiny_dnn;
using namespace tiny_dnn::activation;
using namespace tiny_dnn::layers;

namespace test {

TEST(XML, NN) {
  network<sequential> net;

  net << fc(32 * 32, 300) << sigmoid() << fc(300, 10);

  ASSERT_EQ(net.in_data_size(),32 * 32);
}

} // namespace test

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
