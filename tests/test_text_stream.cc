#include <gtest/gtest.h>

#include "../src/datastream/text_stream.hpp"
#include "../src/common.hpp"

namespace test {

using namespace fstore;
using namespace fstore::datastream;

std::tuple<u64,u64> parse(const std::string &line) {
  std::istringstream ss(line);
  u64 key, val;
  ss >> key; ss >> val;
  return std::make_tuple(key,val);
}

TEST(Stream,text) {
  using TI = TextIter<u64,u64,parse>;
  TI ti("tests/test.txt");

  u64 num(0);
  std::vector<u64> keys;
  for(ti.begin();ti.valid();ti.next()) {
    keys.push_back(ti.key());
    num += 1;
  }
  ASSERT_EQ(num,4);

  ASSERT_EQ(keys[0],12);
  ASSERT_EQ(keys[1],14);
  ASSERT_EQ(keys[2],16);
  ASSERT_EQ(keys[3],12);
}

} // test
