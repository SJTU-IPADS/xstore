#include <gtest/gtest.h>

#include "../src/rdma/connect_handlers.hpp"
#include "../src/rdma/connect_manager.hpp"
#include "../src/rdma/single_op.hpp"
#include "../src/timer.hpp"

namespace test {

const int TCP_PORT = 9999;
const int GLOBAL_MR_ID = 73;

using namespace rdmaio;
using namespace r2::rdma;
using namespace r2;

RdmaCtrl ctrl(TCP_PORT);

class CMTest : public testing::Test
{
protected:
  void SetUp() override {}

  void TearDown() override {}
};

TEST_F(CMTest, Basic)
{
  SyncCM cm(::rdmaio::make_id("localhost", TCP_PORT));
  Timer t;
  auto res = cm.get_mr(
    12, 1000); // get an arbitrary mr with id 12, it should not exsists
  ASSERT_EQ(std::get<0>(res), WRONG_ARG);
}

TEST_F(CMTest, Timeout)
{
  SyncCM cm(::rdmaio::make_id("localhost", TCP_PORT));
  cm.set_timeout(1000);

  const usize REQ_ID = 4;
  ctrl.register_handler(REQ_ID, [](const Buf_t& req) -> Buf_t {
    sleep(2);
    return "";
  });

  auto ret = cm.execute<u64, u64>(
    12, [](const u64& test, const MacID& id) -> std::tuple<IOStatus, u64> {
      u64 dummy_res;
      SimpleRPC sr(std::get<0>(id), std::get<1>(id));
      ASSERT(sr.valid());

      Buf_t reply;
      sr.emplace(
        (u8)REQ_ID, Marshal::serialize_to_buf(static_cast<u64>(test)), &reply);
      auto ret = sr.execute(0, ::rdmaio::default_timeout);
      return std::make_pair(ret, dummy_res);
    });
  ASSERT_EQ(std::get<0>(ret), TIMEOUT);
}

}