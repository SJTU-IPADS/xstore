#include <gtest/gtest.h>

#include "../../deps/rlib/core/lib.hh"
#include "../src/batch_rw_op.hh"

using namespace xstore;
using namespace xcomm;

namespace test {

TEST(BatchRW, Basic) {
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

  SScheduler ssched;
  ssched.spawn([qpp, src_loc, dst_loc](R2_ASYNC) {
    u64 *dst = (u64 *)dst_loc;
    u64 *src = (u64 *)src_loc;
    for (uint i = 0; i < 4; ++i) {
      ASSERT(*(src + i) == 0);
      *(src + i) = i + 73;
      ASSERT(*(dst + i) != *(src + i));
    }

    BatchOp<16> reqs;
    for (uint i = 0; i < 4; ++i) {
      reqs.emplace();
      reqs.get_cur_op()
          .set_rdma_addr(i * sizeof(u64), qpp->remote_mr.value())
          .set_read()
          .set_payload(dst + i, sizeof(u64), qpp->local_mr.value().lkey);
    }
    // flush
    auto ret = reqs.execute_async(qpp, R2_ASYNC_WAIT);
    ASSERT(ret == ::rdmaio::IOCode::Ok);

    // check res
    for (uint i = 0; i < 4; ++i) {
      ASSERT(*(dst + i) == *(src + i)) << *(dst + i);
      *(src + i) = 128 + i;
      ASSERT(*(dst + i) != *(src + i));
    }

    for (uint i = 4; i < 10; ++i) {
      *(src + i) = 128 + i;
      ASSERT(*(dst + i) != *(src + i));
    }
    {
      //BatchOp<16> reqs;
      // test second pass
      for (uint i = 0; i < 10; ++i) {
        reqs.emplace();
        reqs.get_cur_op()
            .set_rdma_addr(i * sizeof(u64), qpp->remote_mr.value())
            .set_read()
            .set_payload(dst + i, sizeof(u64), qpp->local_mr.value().lkey);
      }
      ret = reqs.execute_async(qpp, R2_ASYNC_WAIT);
      ASSERT(ret == ::rdmaio::IOCode::Ok);

      for (uint i = 0; i < 10; ++i) {
        ASSERT(*(dst + i) == *(src + i))
            << *(dst + i) << " " << *(src + i) << " (dst, src)"
            << " @" << i;
      }
    }
    LOG(4) << "test batch done";

    R2_STOP();
    R2_RET;
  });
  ssched.run();
}

} // namespace test

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
