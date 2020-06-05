#include <gtest/gtest.h>

#include "../../deps/kvs-workload/static_loader.hh"
#include "../../deps/tiny-dnn/tiny_dnn/tiny_dnn.h"
#include "../../deps/r2/src/common.hh"

using namespace tiny_dnn;
using namespace tiny_dnn::activation;
using namespace tiny_dnn::layers;

namespace test {

using namespace kvs_workloads;

TEST(XML, NN) {
  network<sequential> net;

  net << fc(1 * 32, 300) << sigmoid() << fc(300, 1);

  std::vector<u64> train_set;
  std::vector<u64> label;

  auto data = StaticLoader::load_from_file("./test_dataset.txt");
  LOG(4) << "total: " << data->size() << " loaded";
}

} // namespace test

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
