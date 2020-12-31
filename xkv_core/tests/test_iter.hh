#pragma once

#include <gtest/gtest.h>
#include <vector>

namespace test {

using namespace xstore;

template <typename Iter>
auto test_iter(const std::vector<XKey> &all_keys, Iter &iter) {
  usize count = 0;
  for (iter.begin(); iter.has_next(); iter.next()) {
    auto key = iter.cur_key();
    ASSERT_EQ(key, all_keys[count]);
    ASSERT_EQ(count, iter.opaque_val());
    count += 1;
  }

  LOG(4) << "count: " << count;
  ASSERT_EQ(count, all_keys.size());
}

} // namespace test
