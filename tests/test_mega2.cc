#include <gtest/gtest.h>

#include "../src/mega_pager_v2.hpp"

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

typedef LeafNode<u64,u64,BlockPager>  Leaf;
typedef NaiveTree<u64,u64,BlockPager> Tree;

template <typename T>
static bool check_increasing_sorted(const std::vector<T> &ids) {
  for(uint i = 1;i < ids.size();++i) {
    if(ids[i] < ids[i-1])
      return false;
  }
  return true;
}

TEST(Mega2, ID) {
#if 1
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

  // then we get all the pages
  std::vector<u32> all_page_ids;
  auto cur_page = t.find_leaf_page(0);
  while(cur_page) {
    all_page_ids.push_back(BlockPager<Leaf>::page_id(cur_page));
    cur_page = cur_page->right;
  }
  ASSERT_FALSE(all_page_ids.empty());
  ASSERT_FALSE(check_increasing_sorted(all_page_ids));

  // finally, we test the megapaer
  MegaPagerV<Leaf> mp;
  std::vector<u64> mega_ids;

  typedef BAddrStream<u64,u64,BlockPager>  BIdStream;

  std::map<u64,u64> faked_learned_index;

  BIdStream id_it(t,0);
  for(id_it.begin();id_it.valid();id_it.next()) {
    auto page_id = id_it.value();
    auto page    = BlockPager<Leaf>::get_page_by_id(BIdStream::PPageID::decode_id(page_id));
    ASSERT_NE(page,nullptr);
    auto mega_id = mp.emplace(BIdStream::PPageID::decode_id(page_id),
                              BIdStream::PPageID::decode_off(page_id),
                              page->num_keys
                              );
    mega_ids.push_back(mega_id);
    faked_learned_index.insert(std::make_pair(id_it.key(),mega_id));
  }
  ASSERT_TRUE(check_increasing_sorted(mega_ids));
  ASSERT_EQ(faked_learned_index.size(),keys.size());

  // finally, we check that, using the mega ids, we can fetch the corresponding values
  for(auto it = faked_learned_index.begin();it != faked_learned_index.end();++it) {
    auto val_p = t.get(it->first);
    ASSERT_NE(nullptr,val_p);

    auto mega_id = it->second;
    auto page_id = mp.get_page_id(mega_id);
    Leaf *page   = BlockPager<Leaf>::get_page_by_id(page_id);
    ASSERT_NE(page,nullptr);

    // the real check happens here
    ASSERT_EQ(page->values[mp.get_offset(mega_id)],*val_p);
  }
  LOG(4) << "all " << mp.total_num() << " pages mapped from [ "
         << faked_learned_index.size() << " ] keys.";

  delete[] buf;
#endif
}

}
