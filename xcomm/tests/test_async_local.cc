#include <gtest/gtest.h>

#include "../src/atomic_rw/local_async_rw_op.hh"

#include "./utils.hh"

namespace test {

using namespace xstore::xcomm::rw;

TEST(Local, Async) {

  auto src_str = random_string(1024);
  auto dest_str = std::string(1024, '\0');

  r2::MemBlock src((void *)src_str.data(), src_str.size());
  r2::MemBlock dest((void *)dest_str.data(), dest_str.size());

  SScheduler ssched;
  ssched.spawn([](R2_ASYNC) {
    // read src into dest

    //LocalRWOp().read(src, dest);

    R2_STOP();
    R2_RET;
  });

  ASSERT_EQ(0, dest_str.compare(src_str));

  ssched.run();
}
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
