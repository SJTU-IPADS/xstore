/// Try evaluating the optimal performance of XStore read/write
/// For each request, the benchmark will do as follows:
/// - 1. read sizeof(XNode) in XTree
/// - 2. read sizeof(payload) given the benchmark payload

#include <gflags/gflags.h>

#include "../lib.hh"

#include "../xcomm/src/atomic_rw/rdma_async_rw_op.hh"
#include "../xcomm/src/atomic_rw/rdma_rw_op.hh"
#include "../xcomm/src/batch_rw_op.hh"
#include "../xcomm/src/lib.hh"

#include "../xkv_core/src/xtree/xnode.hh"

#include "../deps/r2/src/random.hh"
#include "../deps/r2/src/thread.hh"

#include "./reporter.hh"

#include "../deps/rlib/core/lib.hh"

DEFINE_int64(threads, 1, "num client thread to use");
DEFINE_int64(coros, 1, "num client coroutine used per threads");
DEFINE_int64(nic_idx, 0, "which RNIC to use");
DEFINE_int64(payload, 8, "value payload the client would fetch");
DEFINE_string(addr, "localhost:8888", "server address");
DEFINE_int64(emulate_error, 1, "estimated error of the training model");
DEFINE_int64(client_name, 0, "Unique client name (in int)");
DEFINE_int64(reg_mem_name, 73, "The name to register an MR at rctrl.");

namespace bench {

using namespace xstore;
using namespace xcomm;
using namespace rw;
using namespace xstore::bench;
using namespace r2;
using namespace rdmaio;
using namespace rdmaio::qp;

using XThread = ::r2::Thread<usize>;
} // namespace bench

using namespace bench;

const usize kNodeMaxKeys = 16;
using TestTreeNode =
    ::xstore::xkv::xtree::XNode<kNodeMaxKeys, ::xstore::XKey, u64>;

int main(int argc, char **argv) {

  gflags::ParseCommandLineFlags(&argc, &argv, true);

  std::vector<std::unique_ptr<XThread>> workers;

  TestTreeNode node;
  LOG(4) << "Check node, keys:  " << node.keys_start_offset()
         << "; value starts:" << node.value_start_offset()
         << "; total:" << sizeof(TestTreeNode);

  std::vector<Statics> statics(FLAGS_threads);

  for (uint thread_id = 0; thread_id < FLAGS_threads; ++thread_id) {
    workers.push_back(std::move(std::make_unique<XThread>([&statics,
                                                           thread_id]()
                                                              -> usize {
      auto nic =
          RNic::create(RNicInfo::query_dev_names().at(FLAGS_nic_idx)).value();

      auto qp = RC::create(nic, QPConfig()).value();

      // 2. create the pair QP at server using CM
      ConnectManager cm(FLAGS_addr);
      if (cm.wait_ready(1000000, 2) ==
          IOCode::Timeout) // wait 1 second for server to ready, retry 2 times
        RDMA_ASSERT(false) << "cm connect to server timeout";

      auto qp_res =
          cm.cc_rc(FLAGS_client_name + " thread-qp" + std::to_string(thread_id),
                   qp, FLAGS_nic_idx, QPConfig());
      RDMA_ASSERT(qp_res == IOCode::Ok) << std::get<0>(qp_res.desc);

      auto key = std::get<1>(qp_res.desc);
      RDMA_LOG(4) << "t-" << thread_id << " fetch QP authentical key: " << key;

      auto local_mem = ::xstore::Arc<RMem>(new RMem(1024 * 1024 * 20)); // 20M
      auto local_mr = RegHandler::create(local_mem, nic).value();

      auto fetch_res = cm.fetch_remote_mr(FLAGS_reg_mem_name);
      RDMA_ASSERT(fetch_res == IOCode::Ok) << std::get<0>(fetch_res.desc);
      rmem::RegAttr remote_attr = std::get<1>(fetch_res.desc);

      qp->bind_remote_mr(remote_attr);
      qp->bind_local_mr(local_mr->get_reg_attr().value());

      ::r2::util::FastRandom rand(0xdeadbeaf + thread_id + 73 * FLAGS_client_name);

      SScheduler ssched;
      r2::compile_fence();

      for (int i = 0; i < FLAGS_coros; ++i) {
        ssched.spawn([thread_id, qp, i, &statics, &local_mem, &remote_attr,
                      &rand](R2_ASYNC) {
          char *my_buf = (char *)(local_mem->raw_ptr) + 40960 * i;
          ASSERT(40960 >= 16 * sizeof(TestTreeNode))
              << "need buf sz:" << sizeof(TestTreeNode) * 16;

          // use to emulate leaf nodes read
          BatchOp<16> reqs;
          // use to emulate value read
          AsyncOp<1> op;

          while (true) {
            const u64 total_pages = 64 * 1024;
            auto src_slot = rand.next() % (total_pages);
            auto start_addr = src_slot % sizeof(TestTreeNode);
            auto num = rand.rand_number<int>(1, FLAGS_emulate_error + 1) + rand.rand_number<int>(0, kNodeMaxKeys);
            auto page_num = num / kNodeMaxKeys;
            ASSERT(page_num < 16);
            auto end_slot = src_slot + page_num;

            // read page from src_slot -> end_slot
            for (auto addr = src_slot; addr <= end_slot; addr += 1) {
              auto real_addr = addr * sizeof(TestTreeNode);
              // auto real_addr = 0;
              reqs.emplace();
              reqs.get_cur_op()
                  .set_rdma_addr(real_addr, qp->remote_mr.value())
                  .set_read()
                  .set_payload((const u64 *)my_buf,
                               TestTreeNode::value_start_offset(),
                               qp->local_mr.value().lkey);
            }

            // issue
            auto ret = reqs.execute_async(qp, R2_ASYNC_WAIT);
            ASSERT(ret == ::rdmaio::IOCode::Ok)
                << "poll comp error:" << RC::wc_status(ret.desc);

            statics[thread_id].increment();

            // then read the value
            op.set_read()
                .set_rdma_addr(src_slot * sizeof(TestTreeNode),
                               qp->remote_mr.value())
                .set_payload((const u64 *)my_buf, FLAGS_payload,
                             qp->local_mr.value().lkey);
            ret = op.execute_async(qp, IBV_SEND_SIGNALED, R2_ASYNC_WAIT);
            ASSERT(ret == ::rdmaio::IOCode::Ok);
          }
          if (i == FLAGS_coros - 1)
            R2_STOP();
          R2_RET;
        });
      }
      ssched.run();

      return 0;
    })));
  }

  for (auto &w : workers) {
    w->start();
  }

  Reporter::report_thpt(statics, 30);

  for (auto &w : workers) {
    w->join();
  }
}
