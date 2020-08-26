#include <gtest/gtest.h>

#include "../src/transport/rdma_ud_t.hh"

using namespace xstore::transport;

TEST(transport, UDBasic) {

  RCtrl ctrl(8888);

  auto nic_for_sender = RNic::create(RNicInfo::query_dev_names().at(0)).value();
  UDTransport t(nic_for_sender, QPConfig());
  ASSERT(t.connect("localhost:8888","test_server",73) == IOCode::Ok) << " connect failure";

}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
