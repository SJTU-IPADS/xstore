#include <gtest/gtest.h>

#include "../rtm.hh"

#include "../../deps/r2/src/thread.hh"

namespace test {

using namespace xstore::util;

using T = ::r2::Thread<usize>;

TEST(Util, Should_fail) {

  usize a = 1000;
  usize b = 1000;
  const usize iters = 5000000;

  auto T0 = T([&a, &b]() -> usize {
    for (uint i = 0; i < iters; ++i) {
      auto val = 12;
      if ((a > b) && (a - b > 100)) {
        a -= val;
        b += val;
      } else {
        a += val;
        b -= val;
      }
      ASSERT(a >= val && b >= val) << a << " " << b << " " << val;
    }
    return 0;
  });

  auto T1 = T([&a, &b]() -> usize {
    for (uint i = 0; i < iters; ++i) {
      auto val = 12;
      if ((a > b) && (a - b > 100)) {
        a -= val;
        b += val;
      } else {
        a += val;
        b -= val;
      }
      ASSERT(a >= val && b >= val) << a << " " << b;
    }
    return 0;
  });

  T0.start();
  T1.start();

  T0.join();
  T1.join();

  ASSERT_NE(a + b, 2000);
}

TEST(Util, RTM) {

  usize a = 1000;
  usize b = 1000;
  const usize iters = 5000000;

  SpinLock fallback;

  auto T0 = T([&a, &b,&fallback]() -> usize {
    for (uint i = 0; i < iters; ++i) {
      auto val = 12;
      {
        RTMScope rtm(&fallback);
        if ((a > b) && (a - b > 100)) {
          a -= val;
          b += val;
        } else {
          a += val;
          b -= val;
        }
      }
      ASSERT(a >= val && b >= val);
    }
    return 0;
  });

  auto T1 = T([&a, &b,&fallback]() -> usize {
    for (uint i = 0; i < iters; ++i) {
      auto val = 12;
      {
        RTMScope rtm(&fallback);
        if ((a > b) && (a - b > 100)) {
          a -= val;
          b += val;
        } else {
          a += val;
          b -= val;
        }
      }
      ASSERT(a >= val && b >= val) << a << " " << b;
    }
    return 0;
  });

  T0.start();
  T1.start();

  T0.join();
  T1.join();

  ASSERT_EQ(a + b, 2000);
}

} // namespace test

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
