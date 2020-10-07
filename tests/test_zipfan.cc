#include <gtest/gtest.h>

#include "utils/zipfan.hpp"

#include <vector>

using namespace fstore;
using namespace fstore::utils;

namespace test {

TEST(Util,Zipfan) {

  const int total_keys = 64;
  ZipFanD zipfan(total_keys);

  std::vector<u64> counts(total_keys,0);
  ASSERT_EQ(counts.size(),total_keys);

  for(auto i : counts)
    ASSERT_EQ(0,i);

  for(uint i = 0;i < 20000;++i) {
    auto next = zipfan.next();
    ASSERT_GE(next,0);
    ASSERT_LT(next,total_keys);
    counts[next] += 1;
  }
#if 0
  for(uint i = 0;i < counts.size();++i) {
    LOG(4) << "k [" << i << "]: frequency: " << counts[i];
  }
#endif
}

}
