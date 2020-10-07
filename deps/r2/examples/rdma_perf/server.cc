#include "all.hpp"
#include "scheduler.hpp"
#include "futures/rdma_future.hpp"

#include "rlib/rdma_ctrl.hpp"

#include <gflags/gflags.h>

using namespace r2;
using namespace rdmaio;

DEFINE_string(server_ip,"localhost","The server ip to locate.");
DEFINE_int32(server_port,8888,"The server port to locate.");

DEFINE_int64(packet_sz,64,"The payload of each RDMA request.");
DEFINE_int32(buf_g,1,"The total memory to registered, in sz of G.");

DEFINE_int32(threads,1,"Number of QPs created for the server.");

constexpr uint64_t G = 1024 * 1024 * 1024;

/**
 * The RDMA server. The logic is pretty simple, it creates dedicated QPS,
 * and just sleep. The NIC do all the other stuffs.
 */
int main(int argc,char **argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  RdmaCtrl ctrl(FLAGS_server_port);
  char *test_buffer = new char[FLAGS_buf_g * G];

  ASSERT(test_buffer != nullptr);

  auto all_devices = RNicInfo::query_dev_names();
  if(all_devices.empty()) {
    LOG(4) << "No RDMA devices found at this mac!";
    return -1;
  }

  RNic nic(all_devices[0]);
  ASSERT((ctrl.mr_factory.register_mr(global_mr_id,test_buffer,FLAGS_buf_g * G,nic)) == SUCC);

  auto local_mem_p = ctrl.mr_factory.get_mr(global_mr_id);
  ASSERT(local_mem_p != nullptr);

  // fetch the local mr attr, although it is not required
  auto local_mem_attr = local_mem_p->get_attr();

  for(uint i = 0;i < FLAGS_threads;++i) {
    RCQP *qp = new RCQP(nic,local_mem_attr,local_mem_attr,QPConfig());
    ASSERT(ctrl.qp_factory.register_rc_qp(i,qp));
  }

  compile_fence();

  LOG(4) << "init done, now server run on ["
         << FLAGS_server_ip << ","
         << FLAGS_server_port << "]";

  // just sleep
  while(1) {
    sleep(1);
  }
}
