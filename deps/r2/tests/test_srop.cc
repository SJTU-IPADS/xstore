#include <gtest/gtest.h>

#include "../src/rdma/connect_manager.hpp"
#include "../src/rdma/single_op.hpp"

namespace test
{

const int TCP_PORT = 9999;
const int GLOBAL_MR_ID = 73;

using namespace rdmaio;
using namespace r2::rdma;
using namespace r2;

extern RdmaCtrl ctrl;

TEST(RDMA, sop)
{
  u64 remote_addr = 0x12;
  char *test_buffer = new char[1024];
  Marshal::serialize_to_buf<u64>(73, test_buffer + remote_addr);

  auto all_devices = RNicInfo::query_dev_names();
  ASSERT_FALSE(all_devices.empty());

  RNic nic(all_devices[0]);

  {
    ASSERT_EQ(
        (ctrl.mr_factory.register_mr(GLOBAL_MR_ID, test_buffer, 1024, nic)),
        SUCC);

    SyncCM cm(::rdmaio::make_id("localhost", TCP_PORT));
    auto res = cm.get_mr(GLOBAL_MR_ID);
    ASSERT_EQ(std::get<0>(res), SUCC);

    // further check the contents of result
    RemoteMemory::Attr local;
    ctrl.mr_factory.fetch_local_mr(GLOBAL_MR_ID, local);
    ASSERT_EQ(local.key, std::get<1>(res).key);

    RCQP *qp = new RCQP(nic, std::get<1>(res), local, QPConfig());
    ASSERT_TRUE(qp->valid());
    ASSERT(ctrl.qp_factory.register_rc_qp(0, qp));

    ASSERT_EQ(cm.connect_for_rc(qp, 0, QPConfig()), SUCC);

    RScheduler r;
    r.spawnr([&, qp, test_buffer](R2_ASYNC) {
      ::r2::rdma::SROp op(qp);
      op.set_payload(test_buffer, sizeof(u64));
      op.set_remote_addr(remote_addr).set_op(IBV_WR_RDMA_READ);

      ASSERT_NE(*((u64 *)test_buffer), 73);
      R2_EXECUTOR.wait_for(100000);
      auto ret = op.execute(R2_ASYNC_WAIT);
      ASSERT_EQ(std::get<0>(ret), SUCC);

      r2::compile_fence();
      ASSERT_EQ(*((u64 *)test_buffer), 73);
      R2_STOP();
      R2_RET;
    });
    r.run();
  }

  delete[] test_buffer;
}
} // namespace test