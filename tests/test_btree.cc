#include <gtest/gtest.h>

#include "../src/page_addr.hpp"

#include "../src/stores/naive_tree.hpp"
#include "../src/stores/display.hpp"
#include "../src/stores/tree_iters.hpp"
#include "../src/data_sources/tpcc/stream.hpp"
#include "../src/data_sources/ycsb/stream.hpp"
#include "../deps/r2/src/random.hpp"

#include <map>
#include <algorithm>
#include <random>

using namespace fstore;
using namespace fstore::store;
using namespace fstore::sources::tpcc;
using namespace fstore::sources::ycsb;

using namespace r2;
using namespace r2::util;

namespace test {

TEST(Tree, Leaf) {

  typedef LeafNode<u64,u64,BlockPager> Leaf;
  FastRandom rand(0xdeadbeaf);

  char *buf = new char[1024 * 1024 * 1024];
  BlockPager<LeafNode<u64,u64>>::init(buf,1024 * 1024 * 1024);

  for(uint t = 0;t < 20;++t) {
    Leaf l;
    u64 *val;

    for(uint i = 0;i < IM;++i) {
      auto n = l.insert(rand.next() + 1,&val);
      ASSERT_EQ(n,nullptr);
    }

    auto n = l.insert(rand.next() + 1, &val);
    ASSERT_NE(n,nullptr);

    // then we check that the key's content is correct
    u64 min_key = 0;
    ASSERT_EQ(l.num_keys + n->num_keys,IM + 1);

    for(uint i = 0;i < l.num_keys;++i) {
      //ASSERT_GE(l.keys[i],min_key);
      if(l.keys[i] < min_key) {
        LOG(4) << l.to_str();
        ASSERT(false);
      }
      min_key = l.keys[i];
    }
    for(uint i = 0;i < n->num_keys;++i) {
      ASSERT_GE(n->keys[i],min_key);
      min_key = n->keys[i];
    }
  }
  delete[] buf;
}

TEST(Tree,Basics) {

  typedef NaiveTree<u64,u64,BlockPager> Tree;
  typedef LeafNode<u64,u64,BlockPager>  Leaf;

  // first we init the pager
  char *buf = new char[1024 * 1024 * 1024];
  BlockPager<LeafNode<u64,u64>>::init(buf,1024 * 1024 * 1024);

  Tree t; std::map<u64,u64> compare;
  FastRandom rand(0xdeadbeaf);

  /**
   * First, some simple tests
   */
#if 1
  std::vector<u64> simple_keys;
  for(uint i = 0;i < 32;++i) {
    simple_keys.push_back(i);
  }
  //std::shuffle(simple_keys.begin(), simple_keys.end(), std::default_random_engine(0xdeadbeaf));

  for(uint i = 0;i < 32;++i) {
    u64 val = simple_keys[i];
    //LOG(4) << "insert: " << val << " (" << i + 1 << ")";
    t.put(val,val);
    //t.print_tree();
    ASSERT_NE(nullptr,t.get(val));
    ASSERT_EQ(val,*(t.get(val)));
  }

  for(uint i = 0;i < 32;++i) {
    ASSERT_EQ(i,*(t.get(i)));
  }
#endif
  /**
   * Then we use TPC-C's datasets as a large-scale test
   */
  std::vector<u64> keys;

  //StockGenerator generator(1,36);
  YCSBHashGenereator generator(0,1000000);
  for(generator.begin();generator.valid();generator.next()) {
    auto key = generator.key();

    u64 value = rand.next();
    compare.insert(std::make_pair(key,value));
    keys.push_back(key);
  }

  LOG(4) << "total: " << compare.size() << " stocks loaded";
  //std::shuffle(keys.begin(), keys.end(), std::default_random_engine(0xdeadbeaf));
  for(uint i = 0;i < keys.size();++i) {
    auto k = keys[i];
    ASSERT(compare.find(k) != compare.end());
    //LOG(4) << "insert " << k << ";[" << i << ":" << keys.size() << "]";
    t.put(k,compare[k]);
    ASSERT_EQ(*(t.get(k)),compare[k]);
    //t.print_tree();
    //t.sanity_checks();
  }

  r2::compile_fence();

  for(auto it = compare.begin();it != compare.end();++it) {
    auto *val_ptr = t.get(it->first);
    ASSERT_EQ(*val_ptr,it->second);
  }
#if 1
  auto *p = t.find_leaf_page(0);
  int count = 0;
  LOG(4) << "use page size: " << sizeof(Leaf) << "; tree depth: " << t.depth
         << "; max page number supported: "   << BlockPager<Leaf>::max_pages();
  while(p != nullptr && count < 7) {
    LOG(4) << Display<Leaf,BlockPager>::page_to_string(p);
    count++;
    p = p->right;
  }
#endif

  // finally we test the iterators
  typedef BNaiveStream<u64,u64,BlockPager> BStream;
  u64 counter(0), min_key(0);
  BStream it(t,min_key);
  for(it.begin();it.valid();it.next()) {
    auto key = it.key();
    if(key < 32)
      ASSERT_EQ(it.value(),key);
    else {
      ASSERT_EQ(it.value(),compare[key]);
    }
    ASSERT_GE(it.key(),min_key);
    min_key = it.key();
    counter++;
  }
  ASSERT_EQ(counter,compare.size() + 32);
  // end the tests, free resources
  delete[] buf;
}

/**
 * Test the iterator implementation
 */
TEST(Tree,Iters) {

  typedef NaiveTree<u64,u64,BlockPager> Tree;
  typedef LeafNode<u64,u64,BlockPager>  Leaf;

  // first we init the pager
  char *buf = new char[1024 * 1024 * 1024];
  BlockPager<Leaf>::init(buf,1024 * 1024 * 1024);

  Tree t;
  FastRandom rand(0xdeadbeaf);

  StockGenerator generator(1,10);
  for(generator.begin();generator.valid();generator.next()) {
    auto key = generator.key();
    u64 value = rand.next();
    t.put(key.to_u64(),value);
  }

  typedef BNaiveStream<u64,u64,BlockPager> BStream;
  typedef BAddrStream<u64,u64,BlockPager>  BIdStream;
  typedef MegaPager::PPageID               PPageID;

  BStream it(t,0);
  BIdStream id_it(t,0);

  for(it.begin(),id_it.begin(); it.valid() && id_it.valid(); it.next(), id_it.next()) {

    // the keys should match
    ASSERT_EQ(it.key(),id_it.key());
    auto origin_val = it.value();
    auto id = id_it.value();

    auto page_id    = PPageID::decode_id(id);
    auto page_off   = PPageID::decode_off(id);
#if 1
    auto page = BlockPager<Leaf>::get_page_by_id(page_id);
    ASSERT_EQ(page->values[page_off],origin_val);
#endif
  }
  // both iterators should go to the end
  ASSERT_EQ(it.valid(),id_it.valid());

}

} // end namespace test
