#include "all.h"
#include "rdma_ctrl.hpp"
#include "simple_msg.hpp"

#include <gflags/gflags.h>

using namespace rdmaio;

DEFINE_string(server_ip, "localhost", "The server ip to locate");

int main(int argc, char **argv) {

  gflags::ParseCommandLineFlags(&argc, &argv, true);

  char *test_buffer = new char[1024];
  // write something to the test buffer
  Marshal::serialize_to_buf<uint64_t>(0, test_buffer);

  init_device();

  {
    RdmaCtrl ctrl(TCP_PORT);

    // create a session for this connect
    r2::Session session(ctrl);
    session.connect();

    // now we try to post one message to myself
    // ... TODO
    char *local_buf = test_buffer + 512;
    uint64_t before = Marshal::deserialize<uint64_t>(local_buf);
    RDMA_LOG(2) << "Before issue RDMA we get the value: " << before;
#if 1
    ret = qp->send({.op = IBV_WR_RDMA_READ,
                    .flags = IBV_SEND_SIGNALED,
                    .len = sizeof(uint64_t)},
                   {.local_buf = local_buf, .remote_addr = 0, .imm_data = 0});
    RDMA_ASSERT(ret == SUCC);
#endif
    ibv_wc wc;
    ret = qp->wait_completion(wc);
    RDMA_ASSERT(ret == SUCC);

    uint64_t res = Marshal::deserialize<uint64_t>(local_buf);
    RDMA_LOG(2) << "finally we got the value: " << res;

    ctrl.mr_factory.deregister_mr(GLOBAL_MR_ID);

    delete qp;
  }
}
