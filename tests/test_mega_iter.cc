#include <gtest/gtest.h>

#include "mega_iter.hpp"

#include "../src/stores/naive_tree.hpp"
#include "../src/stores/tree_iters.hpp"

// again, we usually prefers the TPCC data sources as our default key distribution.
#include "../src//data_sources/tpcc/stream.hpp"
#include "../deps/r2/src/random.hpp"

#include <algorithm>
#include <random>

using namespace fstore;
using namespace fstore::store;
using namespace fstore::sources::tpcc;

using namespace r2;
using namespace r2::util;

namespace test {

typedef LeafNode<u64,u64,BlockPager>    Leaf;
typedef NaiveTree<u64,u64,BlockPager>   Tree;
typedef BPageStream<u64,u64,BlockPager> PageStream;

TEST(MegaIter, PageIter) {
  // first we init the buffer-pool to store all the leaf pages.
  char *buf = new char[1024 * 1024 * 1024];
  BlockPager<Leaf>::init(buf,1024 * 1024 * 1024);

  Tree t;
  FastRandom rand(0xdeadbeaf);

  std::vector<u64> keys;
  StockGenerator generator(1,10);
  for(generator.begin();generator.valid();generator.next()) {
    keys.push_back(generator.key().to_u64());
  }
  /**
   * We shuffle the all the keys, this is very important.
   * Since otherwise, all the page ids would be sorted.
   */
  std::shuffle(keys.begin(), keys.end(), std::default_random_engine(0xdeadbeaf));
  for(auto k : keys)
    t.put(k,rand.next());

  // we first iterate through all pages
  std::vector<u32> all_page_ids;
  auto cur_page = t.find_leaf_page(0);
  while(cur_page) {
    all_page_ids.push_back(BlockPager<Leaf>::page_id(cur_page));
    cur_page = cur_page->right;
  }

  // we then test the iterator, which its results should match all_page_ids
  PageStream it(t,0); uint count(0);
  for(it.begin();it.valid();it.next(),count++) {
    auto pp = BlockPager<Leaf>::get_page_by_id(it.key());
    ASSERT_EQ(pp,it.value());
  }

  ASSERT_EQ(count,all_page_ids.size());

  delete[] buf;
}

TEST(MegaIter, MegaIter) {
  // first we init the buffer-pool to store all the leaf pages.
  char *buf = new char[1024 * 1024 * 1024];
  BlockPager<Leaf>::init(buf,1024 * 1024 * 1024);

  Tree t;
  FastRandom rand(0xdeadbeaf);

  std::vector<u64> keys;
  StockGenerator generator(1,10);
  for(generator.begin();generator.valid();generator.next()) {
    keys.push_back(generator.key().to_u64());
  }
  /**
   * We shuffle the all the keys, this is very important.
   * Since otherwise, all the page ids would be sorted.
   */
  std::shuffle(keys.begin(), keys.end(), std::default_random_engine(0xdeadbeaf));
  std::map<u64,u64> compare_map;
  for(auto k : keys) {
    auto value = rand.next();
    compare_map.insert(std::make_pair(k,value));
    t.put(k,value);
  }

  std::vector<u32> all_page_ids;
  auto cur_page = t.find_leaf_page(0);
  while(cur_page) {
    all_page_ids.push_back(BlockPager<Leaf>::page_id(cur_page));
    cur_page = cur_page->right;
  }
  LOG(4) << "total : " << all_page_ids.size() << " allocated";

  // We then test the iterator, which its results should match all_page_ids
  PageStream it(t,0);
  MegaPagerV<Leaf> mp;
  MegaStream<Leaf> mega_it(&it,&mp);

  //std::map<u64,mega_id_t> key_mega;
  std::vector<std::pair<u64,mega_id_t> > key_mega;
  for(mega_it.begin();mega_it.valid();mega_it.next()) {
    key_mega.push_back(std::make_pair(mega_it.key(),mega_it.value()));
  }

  u64 fake_num(0);
  for(auto p : key_mega) {
    auto k = std::get<0>(p);
    auto mega_id = std::get<1>(p);
    if (MegaFaker::is_fake(k)){
      fake_num += 1;
      continue;
    }
    // now we use the mega id to fetch the real content, and compare it with compare map
    auto kk = MegaFaker::decode(k);
    ASSERT_EQ(*(t.get(kk)),compare_map[kk]);
    auto page_id = mp.get_page_id(mega_id);
    ASSERT_NE(page_id,invalid_page_id);
    auto page = BlockPager<Leaf>::get_page_by_id(page_id);
    ASSERT_EQ(page->values[mp.get_offset(mega_id)],compare_map[kk]);
  }

  // excluding the fake keys, the remaining must be all valid keys
#if !FAKE_KEYS
  ASSERT_EQ(fake_num,0);
#endif
  ASSERT_EQ(key_mega.size() - fake_num,compare_map.size());
#if FAKE_KEYS
  ASSERT_EQ(key_mega.size(),all_page_ids.size() * Leaf::page_entry());
#endif

  delete[] buf;
}

} // end namespace test
