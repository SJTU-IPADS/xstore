#include <gtest/gtest.h>

#include "../src/smart_cache.hpp"

#include "../src/stores/naive_tree.hpp"
#include "../src/stores/tree_iters.hpp"

// again, we usually prefers the TPCC data sources as our default key
// distribution.
#include "../deps/r2/src/random.hpp"
#include "../src//data_sources/tpcc/stream.hpp"
#include "../src/data_sources/range_generator.hpp"

#include <algorithm>
#include <random>

using namespace fstore;

using namespace fstore::store;
using namespace fstore::sources;
using namespace fstore::sources::tpcc;

using namespace r2;
using namespace r2::util;

namespace test {

typedef LeafNode<u64, u64, BlockPager> Leaf;
typedef NaiveTree<u64, u64, BlockPager> Tree;
typedef BPageStream<u64, u64, BlockPager> PageStream;

TEST(SCache, basic)
{

  // first we init the buffer-pool to store all the leaf pages.
  char* buf = new char[1024 * 1024 * 1024];
  BlockPager<Leaf>::init(buf, 1024 * 1024 * 1024);

  Tree t;
  std::map<u64, u64> baseline;
  FastRandom rand(0xdeadbeaf);
#if 1
  StockGenerator generator(1, 10);
  for (generator.begin(); generator.valid(); generator.next()) {
    auto value = rand.next();
    t.put(generator.key().to_u64(), value);
    baseline.insert(std::make_pair(generator.key().to_u64(), value));
  }
#else
  RangeGenerator generator(1, 12);
  for (generator.begin(); generator.valid(); generator.next()) {
    auto value = generator.value();
    t.put(generator.key(), value);
    baseline.insert(std::make_pair(generator.key(), value));
  }
#endif

  /// finish data loading
  typedef SCache<u64, u64, Leaf, BlockPager> SC;

  RMIConfig rmi_config;
  RMIConfig::StageConfig first, second;

  first.model_type = RMIConfig::StageConfig::LinearRegression;
  first.model_n = 1;

  second.model_n = 12;
  second.model_type = RMIConfig::StageConfig::LinearRegression;
  rmi_config.stage_configs.push_back(first);
  rmi_config.stage_configs.push_back(second);

  // create our smart cache
  SC sc(rmi_config);

  PageStream it(t, 0);
  // train the smart cache
  sc.retrain(&it, t);

  auto leaf = t.find_leaf_page(0);
  LOG(4) << "leaf page : " << leaf
         << "; id: " << BlockPager<Leaf>::page_id(leaf);

  u64 sum = 0;
  for (auto it = baseline.begin(); it != baseline.end(); ++it) {
    auto key = it->first;
    auto val = it->second;

    auto val_p = t.get(key);
    ASSERT_NE(nullptr, val_p);
    ASSERT_EQ(*val_p, val);

    auto cached_ptr = sc.get(key);
    if (cached_ptr == nullptr) {
      ASSERT(false) << "get an invalid smart key: " << key;
      continue;
    }
    if (cached_ptr != val_p) {
      LOG(4) << "error on key: " << key;
    }
    ASSERT_EQ(cached_ptr, val_p);
    sum += *cached_ptr;
  }
  LOG(4) << "done, all sum: " << sum;

  delete[] buf;
}

}
