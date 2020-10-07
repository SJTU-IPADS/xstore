#include <gtest/gtest.h>

#include "../src/futures/rdma_future.hpp"
#include "../src/scheduler.hpp"

#include "rlib/rdma_ctrl.hpp"

using namespace r2;
using namespace rdmaio;

namespace test
{

const int TCP_PORT = 3333;
const int GLOBAL_MR_ID = 73;

void test_cor(int id, R2_ASYNC)
{
  R2_YIELD;
  R2_RET;
}

TEST(Scheduler, id)
{
  // test whether RScheduler's ID is correct
  RScheduler r;
  for (uint i = 0; i < 12; ++i)
  {
    r.spawnr([i](R2_ASYNC) {
      R2_STOP();

      ASSERT_EQ(i + 1, R2_COR_ID());

      test_cor(12, R2_ASYNC_WAIT);

      R2_YIELD;
      R2_RET;
    });
  }
  r.run();
}

TEST(Scheduler, rdma)
{
  // test whether RSCheduler can scheduler RDMA requests

  char *test_buffer = new char[1024];
  // write something to the test buffer
  Marshal::serialize_to_buf<uint64_t>(73, test_buffer);

  auto all_devices = RNicInfo::query_dev_names();
  ASSERT_FALSE(all_devices.empty());

  // use the first devices found to create QP, register MR
  RNic nic(all_devices[0]);
  {
    // start the controler
    RdmaCtrl ctrl(TCP_PORT);

    // register MR
    ASSERT_EQ(
        (ctrl.mr_factory.register_mr(GLOBAL_MR_ID, test_buffer, 1024, nic)),
        SUCC);

    // fetch the register MR's key
    RemoteMemory::Attr local_mr_attr;
    auto ret = RMemoryFactory::fetch_remote_mr(
        GLOBAL_MR_ID, std::make_tuple("localhost", TCP_PORT), local_mr_attr);
    ASSERT_EQ(ret, SUCC);

    // create QP
    RCQP *qp = new RCQP(nic, local_mr_attr, local_mr_attr, QPConfig());
    ASSERT_TRUE(qp->valid());

    // expose QP so as other QP can connect to it
    RDMA_ASSERT(ctrl.qp_factory.register_rc_qp(0, qp));

    // Fetch the QP attr and make QP connected
    QPAttr attr;
    ret = QPFactory::fetch_qp_addr(
        QPFactory::RC, 0, std::make_tuple("localhost", TCP_PORT), attr);
    ASSERT_EQ(ret, SUCC);
    ASSERT_EQ(qp->connect(attr, QPConfig()), SUCC);

    // now we try to post one message to myself
    // initializer the memory
    char *local_buf = test_buffer + 512;
    uint64_t before = Marshal::deserialize<uint64_t>(local_buf);
    ASSERT_NE(before, 73);

    // we use the coroutine to execute
    RScheduler r;
    // spawn a coroutine
    r.spawnr([qp, local_buf](handler_t &h, RScheduler &r) {
      auto id = r.cur_id();
      // really send the requests
      auto ret = RdmaFuture::send_wrapper(
          r,
          qp,
          id,
          {.op = IBV_WR_RDMA_READ,
           .flags = IBV_SEND_SIGNALED,
           .len = sizeof(uint64_t),
           .wr_id = id},
          {.local_buf = local_buf, .remote_addr = 0, .imm_data = 0});
      ASSERT_EQ(ret, SUCC);
      ret = r.pause_and_yield(h);
      ASSERT_EQ(ret, SUCC);

      // check the value
      uint64_t after = Marshal::deserialize<uint64_t>(local_buf);
      ASSERT_EQ(after, 73);

      r.stop_schedule();
      routine_ret(h, r);
    });
    // run all the coroutines
    r.run();

    uint64_t res = Marshal::deserialize<uint64_t>(local_buf);
    RDMA_LOG(2) << "finally we got the value: " << res;

    ctrl.mr_factory.deregister_mr(GLOBAL_MR_ID);

    delete qp;
  }
}

} // end namespace test
