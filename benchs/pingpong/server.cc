#include "lib.hpp"
#include "rlib/rdma_ctrl.hpp"

#include "utils/all.hpp"

#include <gflags/gflags.h>

using namespace rdmaio;
using namespace fstore::utils;
using namespace pingpong;

RdmaCtrl ctrl(8888);

DEFINE_int64(port, 8888, "Server port used.");
DEFINE_string(host, "localhost", "Server host used.");

void
pp_callback(RPC& rpc, const Req::Meta& ctx, const char* msg, u32 size)
{
  auto factory = rpc.get_buf_factory();
  char* reply_buf = factory.get_inline_buf();
  ASSERT(rpc.reply(ctx, reply_buf, sizeof(u64)) == SUCC);
}

int
main(int argc, char** argv)
{

  gflags::ParseCommandLineFlags(&argc, &argv, true);

  auto all_devices = RNicInfo::query_dev_names();
  ASSERT(!all_devices.empty()) << "RDMA must be supported.";

  RNic nic(all_devices[0]);
  RNic nic2(all_devices[1]);

  char* buffer = new char[20 * GB];
  memset(buffer, 0, 1 * GB);
  ASSERT(buffer != nullptr) << "failed to allocate buffer.";

  ASSERT(Helpers::register_mem(ctrl.mr_factory, nic, buffer, 1 * GB, mr_id))
    << "failed to register memory to nic.";

  ASSERT(
    Helpers::register_mem(ctrl.mr_factory, nic2, buffer, 1 * GB, mr_id + 1))
    << "failed to register memory to nic.";

  RemoteMemory::Attr local_mr;
  auto ret = RMemoryFactory::fetch_remote_mr(
    mr_id, std::make_tuple("localhost", 8888), local_mr);

  auto adapter = Helpers::bootstrap_ud(
    ctrl.qp_factory, nic, make_id(FLAGS_host, FLAGS_port), qp_id, mr_id);
  ASSERT(adapter != nullptr) << "failed to create UDAdapter";

  /**
   * Start the main loop
   */
  RScheduler r;
  RPC rpc(adapter);
  rpc.register_callback(PP_RPC_ID, pp_callback);
  rpc.spawn_recv(r);

  auto qp = new RCQP(nic, local_mr, local_mr, QPConfig());
  auto qp2 = new RCQP(nic2, local_mr, local_mr, QPConfig());
  ctrl.qp_factory.register_rc_qp(0, qp);
  ctrl.qp_factory.register_rc_qp(1, qp2);

  QPAttr attr;
  while (ctrl.qp_factory.fetch_qp_addr(
           QPFactory::RC, 0, std::make_tuple("val01", 8888), attr) != SUCC) {
  }
  ASSERT(qp->connect(attr, QPConfig()) == SUCC);
  while (ctrl.qp_factory.fetch_qp_addr(
           QPFactory::RC, 1, std::make_tuple("val01", 8888), attr) != SUCC) {
  }
  ASSERT(qp2->connect(attr, QPConfig()) == SUCC);
  LOG(4) << "pingpong main server running.";
  u64 prev_counter = 0;
  u64 sum = 0;
  while (1) {
    using rdma_ptr_t = volatile u64*;
    //    using rdma_ptr_t = u64*;
    auto vec = (rdma_ptr_t)buffer;
    auto counter = vec[0];
    r2::compile_fence();
    if (counter > prev_counter) {
      ASSERT(vec[counter + 1] == counter)
        << "counter: " << counter << "; value: " << vec[counter + 1];
      if (counter > prev_counter) {
        //        LOG(4) << "checked counter " << counter;
        sum += (counter + vec[counter + 1]);
        if (counter % 4024000 == 1) {
          LOG(4) << sum;
        }
      }
      prev_counter = counter;
    }
  }

  free(buffer);
  return 0;
}
