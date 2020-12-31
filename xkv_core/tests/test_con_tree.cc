#include <gtest/gtest.h>

#include "../../deps/r2/src/thread.hh"
#include "../../deps/r2/src/random.hh"

#include "../src/xtree_con.hh"

namespace test {

using namespace xstore::util;
using namespace xstore::xkv;
using namespace xstore;

using T = ::r2::Thread<usize>;

TEST(Tree, SingleThread) {
  SpinLock lock;
  ASSERT_FALSE(lock.is_locked());
  r2::compile_fence();
  { RTMScope s(&lock); }
  r2::compile_fence();
  ASSERT_FALSE(lock.is_locked());
}

TEST(Tree, Concurrent) {
  const usize n_test_thread = 4;
  const usize nkeys_inserted = 1000000;

  using Tree = CXTree<16, XKey, u64>;

  Tree t;

  std::vector<std::vector<u64> *> all_keys(n_test_thread,nullptr);
  ASSERT(all_keys.size() == n_test_thread) << all_keys.size();
  for (uint i = 0;i < all_keys.size(); ++i) {
    all_keys[i] = new std::vector<u64>();
  }

  std::vector<std::unique_ptr<T>> workers;
  for (uint i = 0;i < n_test_thread; ++i) {
    workers.push_back(
        std::move(std::make_unique<T>([&t, &all_keys, i]() -> usize {
          r2::util::FastRandom rand(0xdeadbeaf + i * 73);
          for (usize j = 0;j < nkeys_inserted; ++j) {
            // do the insert
            auto key = rand.next();
            t.insert(XKey(key),key + 73);
            all_keys[i]->push_back(key);
            if (j % 1000 == 0) {
              //LOG(4) << "insert: " << j << " done";
            }
          }
          return 0;
        })));
  }

  for (auto &w : workers) {
    w->start();
  }

  for (auto &w : workers) {
    w->join();
  }

  LOG(4) << "all insertion threads join";

  // now check keys
  // TODO
  for (auto &v : all_keys) {
    for (auto &k : *v) {
      ASSERT_EQ(t.get(XKey(k)).value(), k + 73);
    }
  }
}

} // namespace test

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
