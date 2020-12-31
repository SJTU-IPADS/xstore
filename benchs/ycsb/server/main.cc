#include <gflags/gflags.h>

#include "../../../deps/r2/src/timer.hh"

#include "../../../xutils/huge_region.hh"

#include "../../../xutils/local_barrier.hh"

DEFINE_int64(port, 8888, "Server listener (UDP) port.");
DEFINE_int64(threads, 1, "Server threads.");
DEFINE_int64(use_nic_idx, 0, "Which NIC to create QP");
DEFINE_int64(reg_nic_name, 0, "The name to register an opened NIC at rctrl.");
DEFINE_int64(reg_mem_name, 73, "The name to register an MR at rctrl.");
DEFINE_uint64(magic_num, 0xdeadbeaf, "The magic number read by the client");
DEFINE_uint64(alloc_mem_m,
              64,
              "The size of memory to register (in size of MB).");
DEFINE_uint64(nkeys, 1000000, "Number of keys to load");

DEFINE_uint64(ncheck_model, 100, "Number of models to check");
#include "./db.hh"
#include "./worker.hh"

using namespace rdmaio;
using namespace rdmaio::rmem;
using namespace xstore::util;
using namespace xstore;

volatile bool running = true;
volatile bool init = false;

RCtrl ctrl(FLAGS_port);

PBarrier* bar;

int
main(int argc, char** argv)
{
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  bar = new PBarrier(FLAGS_threads + 1);

  const usize MB = 1024 * 1024;
  auto mem = HugeRegion::create(FLAGS_alloc_mem_m * 1024 * 1024L).value();

  // first we open the NIC
  auto all_nics = RNicInfo::query_dev_names();
  {
    for (uint i = 0; i < all_nics.size(); ++i) {
      auto nic = RNic::create(all_nics.at(i)).value();

      // register the nic with name 0 to the ctrl
      RDMA_ASSERT(ctrl.opened_nics.reg(i, nic));
    }
  }

  {
    for (uint i = 0; i < all_nics.size(); ++i) {
      ctrl.registered_mrs.create_then_reg(
        i, mem->convert_to_rmem().value(), ctrl.opened_nics.query(i).value());
    }
  }

  auto workers = bootstrap_workers(FLAGS_threads);
  for (auto& w : workers) {
    w->start();
  }

  RDMA_LOG(2) << "YCSB bench server started!";
  // start the listener thread so that client can communicate w it
  ctrl.start_daemon();

  u64 total_sz = sizeof(DBTree::Leaf) * 15000000L;
  ASSERT(mem->sz > total_sz) << "total sz needed: " << total_sz;
  xalloc = new XAlloc<sizeof(DBTree::Leaf)>((char*)mem->start_ptr(), total_sz);
  db.init_pre_alloced_leaf(*xalloc);

  val_buf = (u64)mem->start_ptr() + total_sz;
  if (FLAGS_vlen) {
    model_buf = val_buf + (1024L * 1024L * 1024L * 6L);
  } else {
    model_buf = val_buf;
  }
  buf_end = (u64)mem->start_ptr() + FLAGS_alloc_mem_m * 1024 * 1024;
  ASSERT(model_buf < buf_end) << "too little model buf";

  // first load DB
  {
    r2::Timer t;
    if (FLAGS_load_from_file) {
      ::xstore::load_from_file(FLAGS_nkeys);
      LOG(2) << "load dataset from " << FLAGS_data_file
             << "in :" << t.passed_msec() << " msecs";

    } else {
      LOG(2) << "load linear dataset in :" << t.passed_msec() << " msecs";
      ::xstore::load_linear(FLAGS_nkeys);
    }
  }

  // then train DB
  {
    r2::Timer t;
    ::xstore::train_db("");
    LOG(2) << "Train dataset in :" << t.passed_msec() << "msecs";
  }

  {
    ::xstore::serialize_db();
  }
  LOG(4) << "index related work done!";
  r2::compile_fence();
  init = true;
  bar->wait();

  // check the xcache
  for (uint i = 0; i < FLAGS_ncheck_model; ++i) {
    if (cache->second_layer.at(i)->max != 0) {
      LOG(0) << "model x : " << i
             << " error:" << cache->second_layer.at(i)->total_error()
             << "; total: " << cache->second_layer.at(i)->max;
    }
  }

  // wait for some seconds if there is worker
  if (FLAGS_threads > 0) {
    for (uint i = 0; i < 50; ++i) {
      // while (1) {
      // server does nothing because it is RDMA
      // client will read the reg_mem using RDMA
      sleep(1);
    }
  }
  r2::compile_fence();
  running = false;
  for (auto& w : workers) {
    w->join();
  }

  RDMA_LOG(4) << "server exit!";
}
