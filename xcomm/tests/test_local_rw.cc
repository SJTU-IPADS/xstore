#include <gtest/gtest.h>

#include "../src/atomic_rw/local_rw_op.hh"
#include "../src/atomic_rw/rw_trait.hh"

#include "./utils.hh"

namespace test {

using namespace xstore::xcomm::rw;

TEST(AtomicRW, local) {
  auto src_str = random_string(1024);
  auto dest_str = std::string(1024,'\0');

  ASSERT_NE(0, dest_str.compare(src_str));

  // read src into dest
  r2::MemBlock src((void *)src_str.data(), src_str.size());
  r2::MemBlock dest((void *)dest_str.data(), dest_str.size());

  LocalRWOp().read(src,dest);
}

} // namespace test

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
