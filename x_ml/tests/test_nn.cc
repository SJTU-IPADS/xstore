#include <gtest/gtest.h>

#include "../../deps/kvs-workload/static_loader.hh"
#include "../../deps/r2/src/common.hh"
#include "../../deps/tiny-dnn/tiny_dnn/tiny_dnn.h"

#include "../../lib.hh"
#include "../src/nn.hh"

using namespace tiny_dnn;
using namespace tiny_dnn::activation;
// using namespace tiny_dnn::layers;

namespace test {

using namespace xstore;

using namespace kvs_workloads;
using namespace xstore::xml;

TEST(XML, MYNN)
{
  set_random_seed(0xdeadbeaf);
  for (uint _i = 0; _i < 3; ++_i) {

    NN<XKey> nn(4);

    // LOG(4) << "before train: " << nn.net;
    auto data = StaticLoader::load_from_file("./test_dataset.txt");
    // LOG(4) << "total: " << data->size() << " loaded";

    std::vector<XKey> t;
    std::vector<u64> l;

    for (uint i = 0; i < data->size(); ++i) {
      t.push_back(XKey((*data)[i]));
      l.push_back(i);
    }

    nn.train(t, l);

    for (uint i = 0; i < data->size(); ++i) {
      auto key = (*data)[i];
      LOG(4) << "my nn predict: " << key << " " << nn.predict(XKey(key));
    }
    // LOG(4) << nn.net;
    LOG(2) << "After train: "
           << " -----------";
  }
}
} // namespace test

int
main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
