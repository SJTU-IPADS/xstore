#include <gtest/gtest.h>

#include "../../deps/kvs-workload/static_loader.hh"
#include "../../deps/tiny-dnn/tiny_dnn/tiny_dnn.h"
#include "../../deps/r2/src/common.hh"

using namespace tiny_dnn;
using namespace tiny_dnn::activation;
//using namespace tiny_dnn::layers;

namespace test {

using namespace kvs_workloads;

TEST(XML, NN) {
  //network<sequential> net;

  //net << fc(1, 300) << sigmoid() << fc(300, 1);
  auto net = make_mlp<sigmoid>({1 * 1, 12, 1});

  std::vector<vec_t> train_set;
  std::vector<vec_t> label;

  auto data = StaticLoader::load_from_file("./test_dataset.txt");
  LOG(4) << "total: " << data->size() << " loaded";

  for (uint i = 0;i < data->size(); ++i) {
    train_set.push_back(vec_t({static_cast<float>((*data)[i])}));
    label.push_back(vec_t({static_cast<float>(i)}));
  }

  adagrad optimizer;
  net.train<mse, adagrad>(optimizer, train_set, label, 30, 50);

  for (uint i = 0;i < data->size(); ++i) {
    auto k = (*data)[i];
    LOG(4) << "predict: " << k << " ->"
           << net.predict(vec_t({static_cast<float>(k)}))[0];
  }
}

} // namespace test

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
