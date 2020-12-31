#include <gflags/gflags.h>

#include "../deps/rlib/core/lib.hh"

#include "../xutils/huge_region.hh"

DEFINE_int64(port, 8888, "Server listener (UDP) port.");
DEFINE_int64(use_nic_idx, 0, "Which NIC to create QP");
DEFINE_int64(reg_nic_name, 0, "The name to register an opened NIC at rctrl.");
DEFINE_int64(reg_mem_name, 73, "The name to register an MR at rctrl.");
DEFINE_uint64(magic_num, 0xdeadbeaf, "The magic number read by the client");
DEFINE_uint64(alloc_mem_m, 64, "The size of memory to register (in size of MB).");

using namespace rdmaio;
using namespace rdmaio::rmem;
using namespace xstore::util;

int main(int argc, char **argv) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    // start a controler, so that others may access it using UDP based channel
    RCtrl ctrl(FLAGS_port);
    RDMA_LOG(4) << "Pingping server listenes at localhost:" << FLAGS_port;

    const usize MB = 1024 * 1024;

    // first we open the NIC
    {
        auto nic =
            RNic::create(RNicInfo::query_dev_names().at(FLAGS_use_nic_idx))
                .value();

        // register the nic with name 0 to the ctrl
        RDMA_ASSERT(ctrl.opened_nics.reg(FLAGS_reg_nic_name, nic));
    }

    {
      auto mem = HugeRegion::create(FLAGS_alloc_mem_m * 1024 * 1024).value();
      ctrl.registered_mrs.create_then_reg(
          FLAGS_reg_mem_name, mem->convert_to_rmem().value(),
          ctrl.opened_nics.query(FLAGS_reg_nic_name).value());

      // allocate a memory so that remote QP can access it
      //RDMA_ASSERT(ctrl.registered_mrs.create_then_reg(
      //FLAGS_reg_mem_name, Arc<RMem>(new RMem(FLAGS_alloc_mem_m * MB)),
      //ctrl.opened_nics.query(FLAGS_reg_nic_name).value()));
    }

    // initialzie the value so as client can sanity check its content
    u64 *reg_mem = (u64 *)(ctrl.registered_mrs.query(FLAGS_reg_mem_name)
                               .value()
                               ->get_reg_attr()
                               .value()
                               .buf);
    // start the listener thread so that client can communicate w it
    ctrl.start_daemon();

    RDMA_LOG(2) << "thpt bench server started!";
    // run for 20 sec
    for (uint i = 0; i < 2000; ++i) {
        // server does nothing because it is RDMA
        // client will read the reg_mem using RDMA
        sleep(1);
    }
    RDMA_LOG(4) << "server exit!";
}
