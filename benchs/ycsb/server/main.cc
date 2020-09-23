#include <gflags/gflags.h>

#include "../../../deps/r2/src/timer.hh"

#include "../../../xutils/huge_region.hh"

DEFINE_int64(port, 8888, "Server listener (UDP) port.");
DEFINE_int64(nthreads, 1, "Server threads.");
DEFINE_int64(use_nic_idx, 0, "Which NIC to create QP");
DEFINE_int64(reg_nic_name, 0, "The name to register an opened NIC at rctrl.");
DEFINE_int64(reg_mem_name, 73, "The name to register an MR at rctrl.");
DEFINE_uint64(magic_num, 0xdeadbeaf, "The magic number read by the client");
DEFINE_uint64(alloc_mem_m, 64, "The size of memory to register (in size of MB).");
DEFINE_uint64(nkeys, 1000000, "Number of keys to laod");

#include "./db.hh"
#include "./worker.hh"

using namespace rdmaio;
using namespace rdmaio::rmem;
using namespace xstore::util;
using namespace xstore;

volatile bool running = true;

RCtrl ctrl(FLAGS_port);

int main(int argc, char **argv) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    // first load DB
    {
      r2::Timer t;
      ::xstore::load_linear(FLAGS_nkeys);
      LOG(2) << "load linear dataset in :" << t.passed_msec() << " msecs";
    }

    // then train DB
    {
      r2::Timer t;
      ::xstore::train_db("");
      LOG(2) << "Train dataset in :" << t.passed_msec() << "msecs";
    }

    // start a controler, so that others may access it using UDP based channel

    const usize MB = 1024 * 1024;

    // first we open the NIC
    auto all_nics = RNicInfo::query_dev_names();
    {
      for (uint i = 0;i < all_nics.size();++i) {
        auto nic = RNic::create(all_nics.at(i)).value();

        // register the nic with name 0 to the ctrl
        RDMA_ASSERT(ctrl.opened_nics.reg(i, nic));
      }
    }

    {
      auto mem = HugeRegion::create(FLAGS_alloc_mem_m * 1024 * 1024).value();
      for (uint i = 0; i < all_nics.size(); ++i) {
        ctrl.registered_mrs.create_then_reg(FLAGS_reg_mem_name,
                                            mem->convert_to_rmem().value(),
                                            ctrl.opened_nics.query(i).value());
      }
    }

    // initialzie the value so as client can sanity check its content
    u64 *reg_mem = (u64 *)(ctrl.registered_mrs.query(FLAGS_reg_mem_name)
                               .value()
                               ->get_reg_attr()
                               .value()
                               .buf);
    // start the listener thread so that client can communicate w it
    ctrl.start_daemon();

    auto workers = bootstrap_workers(FLAGS_nthreads);
    for (auto &w : workers) {
      w->start();
    }

    RDMA_LOG(2) << "YCSB bench server started!";
    // run for 20 sec
    for (uint i = 0; i < 10; ++i) {
        // server does nothing because it is RDMA
        // client will read the reg_mem using RDMA
        sleep(1);
    }
    r2::compile_fence();
    running = false;
    for (auto &w : workers) {
      w->join();
    }

    RDMA_LOG(4) << "server exit!";
}
