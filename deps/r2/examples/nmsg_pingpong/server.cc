#include "lib.hpp"
#include "rlib/rdma_ctrl.hpp"

#include "utils/all.hpp"

#include <gflags/gflags.h>

using namespace rdmaio;

int main() {
  char *test_buffer = new char[1024];
  // write something to the test buffer
  Marshal::serialize_to_buf<uint64_t>(73, test_buffer);

  auto all_devices = RNicInfo::query_dev_names();
  if (all_devices.empty()) {
    RDMA_LOG(4) << "No devices found at this mac!";
    return -1;
  }

  // use the first devices found
  RNic nic(all_devices[0]);

  {
    RdmaCtrl ctrl(TCP_PORT);
    RDMA_ASSERT((ctrl.mr_factory.register_mr(GLOBAL_MR_ID, test_buffer, 1024,
                                             nic)) == SUCC);

    RemoteMemory::Attr local_mr_attr;
    RCQP *qp = new RCQP(nic, local_mr_attr, local_mr_attr, QPConfig());
    RDMA_ASSERT(qp->valid());

    // dummy spin
    while (1)
      sleep(1);
  }
}