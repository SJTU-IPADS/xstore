#include "rlib/rc.hpp"
#include "../src/random.hpp"
#include "../src/logging.hpp"

#include <gtest/gtest.h>

using namespace rdmaio;
using namespace r2::util;

namespace test {

TEST(RdmaTest, Progress) {

  auto limit = std::numeric_limits<uint32_t>::max();

  Progress progress;

  progress.forward(12);
  ASSERT_EQ(progress.pending_reqs(),12);

  progress.done(12);
  ASSERT_EQ(progress.pending_reqs(),0);

  // now we run a bunch of whole tests
  FastRandom rand(0xdeadbeaf);
  uint64_t sent = 0;
  uint64_t done = 0;

  uint64_t temp_done = 0;
#if 0 // usually we donot do this test, since it's so time consuming
  Progress p2;

  while(sent <=
        static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) * 64) {
    int to_send = rand.rand_number(12, 4096);
    p2.forward(to_send);

    sent += to_send;
    ASSERT_EQ(p2.pending_reqs(),to_send);

    int to_recv = rand.rand_number(0,to_send);
    while(to_send > 0) {
      temp_done += to_recv;
      p2.done(temp_done);
      ASSERT(to_send >= to_recv) << "send: " << to_send <<";"
                                 << "recv: " << to_recv;
      ASSERT_EQ(p2.pending_reqs(),to_send - to_recv);
      to_send -= to_recv;

      to_recv = rand.rand_number(0, to_send);
    }
  }
  ASSERT_EQ(p2.pending_reqs(),0);
#endif
}

} // end namespace test
