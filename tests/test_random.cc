#include <gtest/gtest.h>
#include "../deps/r2/src/common.hpp"
#include "../deps/r2/src/random.hpp"

using namespace r2::util;

namespace test {

TEST(Random, string) {
  FastRandom rand(0xdeadbeaf);
  for(uint i = 0; i < 100;++i) {
    auto len = rand.rand_number(12, 128);
    ASSERT_LE(len,128);
    ASSERT_GE(len,12);
    auto s = rand.next_string(len);
    auto data = s.data();
    ASSERT_EQ(s.size(),len);
    ASSERT_EQ(data[s.size()],'\0');
    //LOG(4) << "random str: " << s;
  }
}

}
