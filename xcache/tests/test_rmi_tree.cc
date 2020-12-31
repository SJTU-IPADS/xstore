#include <gtest/gtest.h>

#include "../src/logic_addr.hh"
#include "../src/page_tt_iter.hh"
#include "../src/rmi_2.hh"

#include "../../xkv_core/src/xtree/page_iter.hh"

#include "../../x_ml/src/lr/mod.hh"
#include "../../x_ml/src/lr/compact.hh"

#include "../../deps/r2/src/random.hh"

#include "../src/samplers/page_sampler.hh"

// test whether RMI is able to find KV-pairs in a XTree
namespace test {

using namespace xstore;
using namespace xstore::xcache;

r2::util::FastRandom rand(0xdeadbeaf);

TEST(XCache, TT) {
  std::vector<u64> labels;

  using Tree = XTree<16, XKey, u64>;
  Tree t;

  const usize num_p = 4; // number of page in the Tree
  for (uint i = 0; i < 16 * num_p; ++i) {
    t.insert(XKey(i), i);
  }

  XCacheTT tt;

  // add to the training-set
  auto it = XCacheTreeIter<16, XKey, u64>::from_tt(t, &tt);

  usize key_count = 0;
  usize key_count1 = 0;

  // First we check that the keys returned from TT should be the same as
  // it_compare
  auto it_compare = XTreeIter<16, XKey, u64>::from(t);
  it_compare.begin();
  for (it.begin(); it.has_next(); it.next()) {
    ASSERT_TRUE(it_compare.has_next());
    ASSERT_EQ(it_compare.cur_key(), it.cur_key());
    key_count += 1;
    it_compare.next();
  }
  ASSERT_EQ(key_count, num_p * 16);

  // then we verify that the TT entries should equal to the page num, i.e.,
  // total #leaf nodes in the tree
  auto it_p = XTreePageIter<16,XKey,u64>::from(t);
  usize page_count = 0;
  for (it_p.begin(); it_p.has_next(); it_p.next()) {
    ASSERT_EQ(tt.get_wo_incar(page_count), it_p.opaque_val());
    page_count += 1;
  }
  ASSERT_EQ(tt.size(), page_count);

  // finally, we test that whether the logic id is correct with TT entry
  for (it.seek(XKey(0),t); it.has_next(); it.next()) {
    auto id = it.opaque_val() / 16;
    ASSERT_EQ(tt.get_wo_incar(id), reinterpret_cast<TTEntryType>(it.prev_node));
  }
}

template <typename K> using PS = PageSampler<16, K>;

TEST(RMI, Tree) {
  std::vector<u64> all_keys;
  int num_keys = 2048;

  // 12 means all keys num
  for (uint i = 0; i < num_keys; ++i) {
    all_keys.push_back(rand.next());
    // all_keys.push_back(i);
  }
  std::sort(all_keys.begin(), all_keys.end());

  using Tree = XTree<16, XKey, u64>;
  Tree t;

  for (auto k : all_keys) {
    t.insert(XKey(k), k);
  }

  const usize num_rmi = 12;
  LocalTwoRMI<LR, LR, XKey> rmi_idx(num_rmi);
  rmi_idx.default_train_first<XTreeIter<16,XKey,u64>>(t);
  DefaultSample<XKey> s;
  Statics statics;

  std::vector<XCacheTT> tts(num_rmi);

  auto trainers = rmi_idx.dispatch_keys_to_trainers<XTreeIter<16,XKey,u64>>(t);
  for (uint i = 0; i < trainers.size(); ++i) {
    auto &trainer = trainers[i];
    auto it = XCacheTreeIter<16,XKey,u64>::from_tt(t, &(tts[i]));
    rmi_idx.second_layer[i] =
        trainer.train_w_it<XCacheTreeIter<16,XKey,u64>, DefaultSample, LR>(it, t,
                                                                       s);
  }
  LOG(4) << "train done";

  for (uint i = 0; i < num_rmi; ++i) {
    LOG(4) << "model #" << i
           << " responsible for keys: " << rmi_idx.second_layer[i]->max
           << "; error: " << rmi_idx.second_layer[i]->total_error() << "; tt sz: " << tts[i].size();
    if (tts[i].size() <= 3) {
      for (auto t : tts[i].entries) {
        LOG(4) << "check tt: " << reinterpret_cast<XNode<16,XKey,u64> *>(t);
      }
    }
  }

  // test that all keys can be found using the predict
  for (auto k : all_keys) {
    auto m = rmi_idx.select_sec_model(XKey(k));
    ASSERT(tts[m].size() > 0);
    auto range = rmi_idx.get_predict_range(XKey(k));

    auto ns = std::max(std::get<0>(range) / 16, 0);
    auto ne = std::min(std::get<1>(range) / 16, static_cast<int>(tts[m].size() - 1));
    bool found = false;
    for (auto p = ns; p <= ne; ++p) {
      XNode<16,XKey,u64> *node = reinterpret_cast<XNode<16,XKey,u64> *>(tts[m].get_wo_incar(p));
      //LOG(4) << "search node: " << node;
      ASSERT_GE(node->num_keys(),0);
      if (node->search(XKey(k))) {
        found = true;
        break;
      }
    }
    ASSERT(found) << "range: " << ns << " " << ne << "; for  k: " << k
                  << "; page sz:" << tts[m].size();
    ASSERT_TRUE(found);
  }
#if 0
  // now use counter iterator to test the accuracy of training
  LocalTwoRMI<LR, LR, XKey> rmi_idx1(num_rmi);
  // avoid training
  rmi_idx1.first_layer.model_from_serialize(rmi_idx.first_layer.model.serialize());
  rmi_idx1.first_layer.up_bound = rmi_idx.first_layer.up_bound;

  auto trainers1 = rmi_idx1.dispatch_keys_to_trainers<XTreeIter<16,XKey,u64>>(t);
  for (uint i = 0; i < trainers1.size(); ++i) {
    auto &trainer = trainers1[i];
    auto it = XTreeIter<16,XKey,u64>::from(t);
    rmi_idx1.second_layer[i] =
        trainer.train_w_it<XTreeIter<16,XKey,u64>, DefaultSample, LR>(it, t, s);
  }

  LOG(4) << "train 1 done";
  for (uint i = 0; i < num_rmi; ++i) {
    LOG(4) << "model #" << i
           << " responsible for keys: " << rmi_idx1.second_layer[i].max
           << "; error: " << rmi_idx1.second_layer[i].total_error();
  }
#endif
  // check other sampler
  LocalTwoRMI<LR, CompactLR,XKey> rmi_idx2(num_rmi);
  std::vector<XCacheTT> tts2(num_rmi);

  rmi_idx2.first_layer.model_from_serialize(
      rmi_idx.first_layer.model.serialize());
  rmi_idx2.first_layer.up_bound = rmi_idx.first_layer.up_bound;

  auto trainers2 = rmi_idx2.dispatch_keys_to_trainers<XTreeIter<16,XKey,u64>>(t);
  for (uint i = 0; i < trainers2.size(); ++i) {
    PageSampler<16,XKey> s;
    auto &trainer = trainers2[i];
    auto it = XCacheTreeIter<16, XKey, u64>::from_tt(t, &(tts2[i]));
    rmi_idx2.second_layer[i] =
        trainer.train_w_it<XCacheTreeIter<16,XKey,u64>, PS, CompactLR>(it, t, s);
  }

  LOG(4) << "train 2 done";
  for (uint i = 0; i < num_rmi; ++i) {
    LOG(4) << "model #" << i
           << " responsible for keys: " << rmi_idx2.second_layer[i]->max
           << "; error: " << rmi_idx2.second_layer[i]->total_error();
    if (tts2[i].size() <= 3) {
      for (auto t : tts2[i].entries) {
        LOG(4) << "check tt: " << reinterpret_cast<XNode<16,XKey,u64> *>(t);
      }
    }
  }

  // finally check pagesampler works correctly
  for (auto k : all_keys) {
    auto m = rmi_idx2.select_sec_model(XKey(k));
    ASSERT(tts2[m].size() > 0);
    auto range = rmi_idx2.get_predict_range(XKey(k));
    //LOG(4) << "p range: " << std::get<0>(range) << " " << std::get<1>(range);
    auto ns = std::max(std::get<0>(range) / 16, 0);
    auto ne =
        std::min(std::get<1>(range) / 16, static_cast<int>(tts2[m].size() - 1));
    bool found = false;
    for (auto p = ns; p <= ne; ++p) {
      auto node = reinterpret_cast<XNode<16, XKey, u64> *>(tts2[m].get_wo_incar(p));
      // LOG(4) << "search node: " << node;
      ASSERT_GE(node->num_keys(), 0);
      if (node->search(XKey(k))) {
        found = true;
        break;
      }
    }
    ASSERT(found) << "range: " << ns << " " << ne << "; for  k: " << k
                  << "; page sz:" << tts2[m].size();
    ASSERT_TRUE(found);
  }

  // finally, we check other update function
  auto trainers3 = rmi_idx2.dispatch_keys_to_trainers<XTreeIter<16,XKey,u64>>(t);
  for (uint i = 0; i < trainers3.size(); ++i) {
    PageSampler<16,XKey> s;
    auto &trainer = trainers3[i];
    tts2[i].clear();
    auto it = XCacheTreeIter<16,XKey,u64>::from_tt(t, &(tts2[i]));
    rmi_idx2.second_layer[i] =
        trainer.train_w_it<XCacheTreeIter<16, XKey,u64>, PS, CompactLR>(
            it, t, s, page_update_func<16>);
  }

  usize tt_sz = 0;
  for (auto &t : tts2) {
    LOG(4) << "tt: en:" << t.size() << "; mem:" << t.mem();
    tt_sz += t.mem();
  }
  LOG(4) << "train 2X done, tree inner sz: " << t.sz_inner() << "; tt sz:"
         << tt_sz;
  for (uint i = 0; i < num_rmi; ++i) {
    LOG(4) << "model #" << i
           << " responsible for keys: " << rmi_idx2.second_layer[i]->max
           << "; error: " << rmi_idx2.second_layer[i]->total_error();
    if (tts2[i].size() <= 3) {
      for (auto t : tts2[i].entries) {
        LOG(4) << "check tt: " << reinterpret_cast<XNode<16,XKey,u64> *>(t);
      }
    }
  }
}

} // namespace test

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
