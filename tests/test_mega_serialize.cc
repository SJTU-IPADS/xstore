#include <gtest/gtest.h>

#include "../src/mega_pager_v2.hpp"
#include "../src/stores/naive_tree.hpp"

#include "../deps/r2/src/random.hpp"

using namespace fstore;
using namespace fstore::store;

namespace test {

typedef LeafNode<u64,u64,BlockPager>  Leaf;

TEST(Mega, Serialzie) {

  r2::util::FastRandom rand(0xdeadbeaf);

  u64 total_entries = 12;
  MegaPagerV<Leaf> test(4096);
  for(uint i = 0; i < total_entries;++i) {
    test.emplace(rand.next(),1,2);
  }

  ASSERT_EQ(test.all_ids_.size(),total_entries);

  MegaPagerV<Leaf> test2(test.serialize_to_buf());
  ASSERT_EQ(test2.all_ids_.size(),test.all_ids_.size());

  for(uint i = 0;i < total_entries;++i) {
    ASSERT_EQ(test2.all_ids_[i],test.all_ids_[i]);
  }
}

} // test
