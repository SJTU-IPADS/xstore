#include <gtest/gtest.h>

#include "../src/common.hpp"
#include "../src/rdma/connect_manager.hpp"

namespace test
{

const int TCP_PORT = 8888;
const int GLOBAL_MR_ID = 73;

using namespace rdmaio;
using namespace r2::rdma;
using namespace r2;

TEST(CM, mr)
{
    char *test_buffer = new char[1024];

    auto all_devices = RNicInfo::query_dev_names();
    ASSERT_FALSE(all_devices.empty());

    RNic nic(all_devices[0]);
    RdmaCtrl ctrl(TCP_PORT);
    ASSERT_EQ(
        (ctrl.mr_factory.register_mr(GLOBAL_MR_ID, test_buffer, 1024, nic)),
        SUCC);
    SyncCM cm(::rdmaio::make_id("localhost", TCP_PORT));

    RemoteMemory::Attr local_mr;
    ASSERT_EQ(ctrl.mr_factory.fetch_local_mr(GLOBAL_MR_ID, local_mr), SUCC);
    auto remote_res = cm.get_mr(GLOBAL_MR_ID);
    ASSERT_EQ(std::get<0>(remote_res), SUCC);

    auto remote_mr = std::get<1>(remote_res);
    ASSERT_EQ(local_mr.buf, remote_mr.buf);
    ASSERT_EQ(local_mr.key, remote_mr.key );

    delete[] test_buffer;
}

} // namespace test