#include <gtest/gtest.h>

#include <vector>

#include "../src/xalloc.hh"

namespace test {

using namespace r2;
using namespace xstore::xkv;

// check whether `addr` intersects with addrs stored in prev
// `addr` intersects with a `p_addr` if and only if:
// p_addr < addr < p_addr + sz
bool check_intersect(std::vector<u64> &prev, const usize &sz, const u64 &addr) {
  for (auto p_addr : prev) {
    if (p_addr < addr && p_addr + sz > addr ) {
      return true;
    }
  }
  return false;
}

TEST(XKV, Alloc) {
  auto total_num = 1000;
  auto total_sz = 256 * total_num;
  auto xalloc = XAlloc<256>(new char[total_sz], total_sz);

  std::vector<u64> prev_alloced;

  int cur_alloced = 0;
  while (cur_alloced < total_num) {
    auto res = xalloc.alloc();
    if (unlikely(!res)) {
      LOG(4) << cur_alloced;
    }
    ASSERT_TRUE(res);
    auto addr = reinterpret_cast<u64>(res.value());
    ASSERT_FALSE(check_intersect(prev_alloced, 256, addr));
    prev_alloced.push_back(addr);
    cur_alloced += 1;
  }
}

}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
