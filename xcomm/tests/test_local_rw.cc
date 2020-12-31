#include <gtest/gtest.h>

#include "../src/atomic_rw/local_rw_op.hh"
#include "../src/atomic_rw/wrapper_type.hh"
#include "../src/atomic_rw/unwrapper_type.hh"

#include "./utils.hh"

namespace test {

using namespace xstore::xcomm::rw;

TEST(AtomicRW, basic_rw) {
  auto src_str = random_string(1024);
  auto dest_str = std::string(1024,'\0');

  ASSERT_NE(0, dest_str.compare(src_str));

  // read src into dest
  r2::MemBlock src((void *)src_str.data(), src_str.size());
  r2::MemBlock dest((void *)dest_str.data(), dest_str.size());

  LocalRWOp().read(src,dest);
  ASSERT_EQ(0, dest_str.compare(src_str));

  inplace_rand_str((char *)dest.mem_ptr,dest.sz);
  ASSERT_NE(0, dest_str.compare(src_str));

  // now write
  LocalRWOp().write(dest,src);
  ASSERT_EQ(0, dest_str.compare(src_str));

  // test wrapped rw
  inplace_rand_str((char *)dest.mem_ptr, dest.sz);
  ASSERT_NE(0, dest_str.compare(src_str));

  OrderedRWOp().write(dest, src);
  ASSERT_EQ(0, dest_str.compare(src_str));
}

TEST(AtomicRW, wrapped_type_create) {
  using WT = WrappedType<u64>;
  WT x(5);
  x.begin_write();
  *(x.get_payload_ptr()) += 1;
  ASSERT_FALSE(x.consistent()); // should be inconsistent during the write
  x.done_write();

  // should be consistent after the done_write
  ASSERT_TRUE(x.consistent());
  // now the value should eq to 6
  ASSERT_EQ(*x.get_payload_ptr(), 6);
}

TEST(AtomicRW, unwrapper_type) {
  using WT = WrappedType<u64>;
  using UWT = UWrappedType<u64>;
  ASSERT_NE(sizeof(WT), sizeof(UWT));
}

} // namespace test

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
