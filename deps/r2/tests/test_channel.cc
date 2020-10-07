#include "../src/channel/channel.hpp"
#include "../src/logging.hpp"
#include "../src/thread.hpp"
#include <gtest/gtest.h>

using namespace r2;

namespace test {

TEST(Channel, Correctness)
{
  uint32_t max_entry_num = 1 << 16;
  Channel<uint64_t> ch(max_entry_num);
  ASSERT_TRUE(ch.isEmpty());
  for (uint32_t i = 0; i < (max_entry_num << 1); i++) {
    bool res = ch.enqueue(i);
    if (i < max_entry_num)
      ASSERT_TRUE(res);
    else
      ASSERT_FALSE(res);
  }

  for (uint32_t i = 0; i < (max_entry_num << 1); i++) {
    uint64_t value = 0;
    auto res = ch.dequeue();
    if (i < max_entry_num) {
      ASSERT_TRUE(res);
      ASSERT_EQ(res.value(), i);
    } else
      ASSERT_FALSE(res);
  }

  ASSERT_TRUE(ch.isEmpty());
}

TEST(Channel, Blocking)
{
  uint32_t max_entry_num = 1 << 16;
  Channel<uint64_t> ch(max_entry_num);
  ASSERT_TRUE(ch.isEmpty());

  Thread<u64> writer([=, &ch]() -> u64 {
    for (uint32_t i = 0; i < (max_entry_num << 2); i++) {
      ch.enqueue_blocking(i);
    }
    return 0;
  });
  Thread<double> reader([=, &ch]() -> double {
    for (uint32_t i = 0; i < (max_entry_num << 2); i++) {
      EXPECT_EQ(i, ch.dequeue_blocking());
    }
  });
  writer.start();
  reader.start();

  writer.join();
  reader.join();

  ASSERT_TRUE(ch.isEmpty());
}

TEST(Channel, NonBlocking)
{
  uint32_t max_entry_num = 1 << 16;
  Channel<uint64_t> ch(max_entry_num);
  ASSERT_TRUE(ch.isEmpty());

  volatile uint32_t counter = 0;
  Thread<double> writer([=, &ch, &counter]() -> double {
    uint64_t val_count = 0;

    for (uint32_t i = 0; i < (max_entry_num << 2); i++) {
      auto res = ch.enqueue(val_count);
      if (res) {
        val_count++;
        __sync_fetch_and_add(&counter, 1);
      }
    }
  });
  Thread<double> reader([=, &ch, &counter]() -> double {
    uint64_t val_count = 0;

    for (uint32_t i = 0; i < (max_entry_num << 2); i++) {
      auto res = ch.dequeue();
      if (res) {
        EXPECT_EQ(val_count++, *res);
        __sync_fetch_and_sub(&counter, 1);
      }
    }
  });
  writer.start();
  reader.start();
  writer.join();
  reader.join();
  ASSERT_EQ(counter, ch.size());
}

} // namespace test