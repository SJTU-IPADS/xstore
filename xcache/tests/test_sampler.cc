#include <gtest/gtest.h>

#include "../src/samplers/step_sampler.hh"
#include "../src/samplers/page_sampler.hh"

#include "../../xkv_core/src/xtree/mod.hh"
#include "../../xkv_core/src/xtree/page_iter.hh"
#include "../../xkv_core/src/xtree/iter.hh"

namespace test {

using namespace xstore::xcache;
using namespace xstore;

using KeyType = XKey;

TEST(Sampler, step) {

  std::vector<KeyType> t_set;
  std::vector<u64> labels;

  StepSampler<XKey> s(1);

  int num = 1024;

  for (uint i = 0; i < num; ++i) {
    s.add_to(XKey(i), i, t_set, labels);
  }

  ASSERT_EQ(t_set.size(), labels.size());
  ASSERT_EQ(t_set.size(), num);

  // step two
  t_set.clear();
  labels.clear();

  StepSampler<XKey> s1(2);

  for (uint i = 0; i < num; ++i) {
    s1.add_to(XKey(i), i, t_set, labels);
  }

  ASSERT_EQ(t_set.size(), labels.size());
  ASSERT_EQ(t_set.size(), num / 2);

  // step four
  t_set.clear();
  labels.clear();

  StepSampler<XKey> s2(4);

  for (uint i = 0; i < num; ++i) {
    s2.add_to(XKey(i), i, t_set, labels);
  }

  ASSERT_EQ(t_set.size(), labels.size());
  ASSERT_EQ(t_set.size(), num / 4);
}

using namespace xstore::xkv::xtree;

TEST(Sampler, Page) {

  std::vector<KeyType> t_set;
  std::vector<u64> labels;

  using Tree = XTree<16, XKey, u64>;
  Tree t;

  const usize num_p = 4; // number of page in the Tree
  for (uint i = 0; i < 16 * num_p; ++i) {
    t.insert(XKey(i), i);
  }

  // add to the training-set

  auto it = XTreeIter<16,XKey,u64>::from(t);
  usize count_page = 0;
  usize key_count = 0;

  StepSampler<XKey> s(1);
  for (it.seek(XKey(0), t); it.has_next(); it.next()) {
    // TODO
    key_count += 1;
    s.add_to(it.cur_key(), it.opaque_val(), t_set, labels);
  }
  ASSERT_EQ(key_count, 16 * num_p);
  ASSERT_EQ(key_count, t_set.size());
  ASSERT_EQ(t_set.size(), labels.size());

  // test another

  PageSampler<16,XKey> p;
  // first we sample all keys
  usize key_count1 = 0;
  auto it1 = XTreePageIter<16, XKey, u64>::from(t);
  for (it1.seek(XKey(0), t); it1.has_next(); it1.next()) {
    count_page += 1;
    auto &p = it1.cur_node.value();
    key_count1 += p.num_keys();
    ASSERT_GE(p.num_keys(), 1);
  }
  ASSERT_EQ(it1.logic_page_id, count_page);
  ASSERT_EQ(key_count, key_count1);

  // then we check whether all the page sample add correct number of keys
  // the assumption is that each node has more than 2 keys
  t_set.clear();
  labels.clear();
  {
    PageSampler<16,XKey> p;
    count_page = 0;
    for (it1.seek(XKey(0), t); it1.has_next(); it1.next()) {
      auto &page = it1.cur_node.value();
      for (uint i = 0; i < 16; ++i) {
        if (page.get_key(i) != XKey(kInvalidKey)) {
          // add
          p.add_to(page.get_key(i),
                   LogicAddr::encode_logic_addr<16>(count_page, i), t_set,
                   labels);
        }
      }
      count_page += 1;
    }
    p.finalize(t_set, labels);
    ASSERT_EQ(
        t_set.size(),
        labels.size()); // the label should have the same num as training-data
    ASSERT_EQ(t_set.size(), count_page * 2);
  }
}

} // namespace test

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
