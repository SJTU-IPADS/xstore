#include "all.h"
#include "rdma_ctrl.hpp"

#include <gflags/gflags.h>

using namespace rdmaio;

DEFINE_string(server_ip,"localhost","The server ip to locate");

int main(int argc,char **argv) {

  gflags::ParseCommandLineFlags(&argc, &argv, true);

  char *test_buffer = new char[1024];
  // write something to the test buffer
  Marshal::serialize_to_buf<uint64_t>(0,test_buffer);

  auto all_devices = RNicInfo::query_dev_names();
  if(all_devices.empty()) {
    RDMA_LOG(4) << "No devices found at this mac!";
    return -1;
  }

  // use the first devices found
  RNic nic(all_devices[0]);
  {
    RdmaCtrl ctrl(TCP_PORT);
    RDMA_ASSERT((ctrl.mr_factory.register_mr(GLOBAL_MR_ID,test_buffer,1024,nic)) == SUCC);

    RemoteMemory::Attr local_mr_attr;
    auto ret = RMemoryFactory::fetch_remote_mr(GLOBAL_MR_ID,
                                               std::make_tuple(FLAGS_server_ip,TCP_PORT),
                                               local_mr_attr);
    RDMA_ASSERT(ret == SUCC);

    RCQP *qp = new RCQP(nic,local_mr_attr,local_mr_attr,QPConfig());
    RDMA_ASSERT(qp->valid());

    RDMA_ASSERT(ctrl.qp_factory.register_rc_qp(0,qp));

    // connect the QP
    QPAttr attr;
    ret = QPFactory::fetch_qp_addr(QPFactory::RC,0,std::make_tuple(FLAGS_server_ip,TCP_PORT),
                                   attr);
    RDMA_ASSERT(ret == SUCC);
    RDMA_LOG(4) << Info::qp_addr_to_str(attr.addr);

    RDMA_ASSERT(qp->connect(attr,QPConfig()) == SUCC);

    // now we try to post one message to myself
    // ... TODO
    char *local_buf = test_buffer + 512;
    uint64_t before = Marshal::deserialize<uint64_t>(local_buf);
    RDMA_LOG(2) << "Before issue RDMA we get the value: " << before;
#if 1
    ret = qp->send(
        {.op = IBV_WR_RDMA_READ,
         .flags = IBV_SEND_SIGNALED,
         .len = sizeof(uint64_t)},
      {.local_buf = local_buf,
       .remote_addr = 0,
       .imm_data = 0});
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
