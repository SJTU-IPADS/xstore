#include <gtest/gtest.h>

#include "../src/atomic_rw/rdma_rw_op.hh"
#include "../src/atomic_rw/rdma_async_rw_op.hh"
#include "../../deps/rlib/core/lib.hh"

#include "./utils.hh"

namespace test {

using namespace xstore::xcomm::rw;

TEST(AtomicRW, BasicRemoteRW) {
  auto res = RNicInfo::query_dev_names();
  ASSERT_FALSE(res.empty());

  auto nic = std::make_shared<RNic>(res[0]);
  ASSERT_TRUE(nic->valid());

  auto config = QPConfig();
  auto qpp = RC::create(nic, config).value();
  ASSERT_TRUE(qpp->valid());

  // try send an RDMA request
  // init the memory
  auto mem = Arc<RMem>(new RMem(4096)); // allocate a memory with 1K bytes
  ASSERT_TRUE(mem->valid());

  // register it to the Nic
  RegHandler handler(mem, nic);
  ASSERT_TRUE(handler.valid());

  auto mr = handler.get_reg_attr().value();

  RC &qp = *qpp;
  qp.bind_remote_mr(mr);
  qp.bind_local_mr(mr);

  // finally connect to myself
  auto res_c = qp.connect(qp.my_attr());
  RDMA_ASSERT(res_c == IOCode::Ok);

  // now the real test
  char *src_loc = reinterpret_cast<char *>(mem->raw_ptr);
  char *dst_loc = src_loc + 2048;

  // we will test using 1024-bytes str
  r2::MemBlock src(0, 1024); // rdma_addr is 0
  r2::MemBlock dest((void *)dst_loc, src.sz);

  // main test body!
  ::test::inplace_rand_str(src_loc, 1024);

  // remote write
  auto ret = RDMARWOp(qpp).read(src, dest);
  ASSERT(ret == ::rdmaio::IOCode::Ok);
  ASSERT_EQ(0, memcmp(src_loc,dst_loc, dest.sz));

  // test async
  const usize coroutines = 12;
  // first init buf
  for (uint i = 0;i < coroutines; ++i) {
    ::test::inplace_rand_str(src_loc + 64 * i, 64);
  }

  SScheduler ssched;
  for (uint i = 0; i < coroutines; ++i) {
    ssched.spawn([qpp, src_loc, dst_loc, i](R2_ASYNC) {
      r2::MemBlock src((void *)(i * 64), 64); // rdma_addr is 0
      r2::MemBlock dest((void *)(dst_loc + i * 64), src.sz);

      auto ret = AsyncRDMARWOp(qpp).read(src, dest,R2_ASYNC_WAIT);
      ASSERT(ret == ::rdmaio::IOCode::Ok);

      ASSERT_EQ(0,memcmp(src_loc + i * 64, dst_loc + i * 64, dest.sz));

      if (i == coroutines - 1) {
        R2_STOP();
      }
      R2_RET;
    });
  }
  ssched.run();
}

}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
