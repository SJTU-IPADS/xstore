#include <gtest/gtest.h>

#include "../src/page_tt_iter.hh"
#include "../src/logic_addr.hh"
#include "../../xkv_core/src/xtree/page_iter.hh"

namespace test {
using namespace xstore::xcache;

TEST(XCache, TT) {
  std::vector<KeyType> t_set;
  std::vector<u64> labels;

  using Tree = XTree<16, u64>;
  Tree t;

  const usize num_p = 4; // number of page in the Tree
  for (uint i = 0; i < 16 * num_p; ++i) {
    t.insert(i, i);
  }

  XCacheTT tt;

  // add to the training-set
  auto it = XCacheTreeIter<16, u64>::from_tt(t, &tt);

  usize key_count = 0;
  usize key_count1 = 0;

  // First we check that the keys returned from TT should be the same as
  // it_compare
  auto it_compare = XTreeIter<16, u64>::from(t);
  it_compare.begin();
  for (it.begin(); it.has_next(); it.next()) {
    ASSERT_TRUE(it_compare.has_next());
    ASSERT_EQ(it_compare.cur_key(), it.cur_key());
    key_count += 1;
    it_compare.next();

    auto v = it.opaque_val();
    LOG(0) << LogicAddr::decode_logic_id(v) << " " << LogicAddr::decode_off(v);
  }
  ASSERT_EQ(key_count, num_p * 16);

  // then we verify that the TT entries should equal to the page num, i.e.,
  // total #leaf nodes in the tree
  auto it_p = XTreePageIter<16, u64>::from(t);
  usize page_count = 0;
  for (it_p.begin(); it_p.has_next(); it_p.next()) {
    page_count += 1;
  }
  ASSERT_EQ(tt.size(), page_count);
}
} // namespace test

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
