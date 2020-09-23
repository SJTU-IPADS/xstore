#include <gflags/gflags.h>

// db schema
#include "./schema.hh"

#include "./proto.hh"

using namespace xstore;

#include "../../xcomm/tests/transport_util.hh"

#include "../../xcomm/src/rpc/mod.hh"
#include "../../xcomm/src/transport/rdma_ud_t.hh"

#include "../../xutils/local_barrier.hh"
#include "../../xutils/marshal.hh"

#include "../../deps/r2/src/thread.hh"
#include "../../deps/r2/src/rdma/async_op.hh"

#include "../../benchs/reporter.hh"

using namespace test;

using namespace xstore::rpc;
using namespace xstore::transport;
using namespace xstore::bench;
using namespace xstore::util;

// prepare the sender transport
using SendTrait = UDTransport;
using RecvTrait = UDRecvTransport<2048>;
using SManager = UDSessionManager<2048>;

// flags
DEFINE_int64(threads, 1, "num client thread used");
DEFINE_int64(coros, 1, "num client coroutine used per threads");
DEFINE_string(addr, "localhost:8888", "server address");

using XThread = ::r2::Thread<usize>;

std::unique_ptr<XCache> cache = nullptr;

using namespace r2::rdma;
DEFINE_int64(client_name, 0, "Unique client name (in int)");

int main(int argc, char **argv) {

  gflags::ParseCommandLineFlags(&argc, &argv, true);

  std::vector<std::unique_ptr<XThread>> workers;

  std::vector<::xstore::bench::Statics> statics(FLAGS_threads);

  PBarrier bar(FLAGS_threads);

  for (uint thread_id = 0; thread_id < FLAGS_threads; ++thread_id) {
    workers.push_back(std::move(std::make_unique<XThread>([&statics, &bar,
                                                           thread_id]()
                                                              -> usize {
      usize nic_idx = 0;
      auto nic_for_sender =
          RNic::create(RNicInfo::query_dev_names().at(nic_idx)).value();
      auto qp = UD::create(nic_for_sender, QPConfig()).value();

      auto mem_region1 = HugeRegion::create(64 * 1024 * 1024).value();
      auto mem1 = mem_region1->convert_to_rmem().value();
      auto handler1 = RegHandler::create(mem1, nic_for_sender).value();
      SimpleAllocator alloc1(mem1, handler1->get_reg_attr().value());
      auto recv_rs_at_send =
          RecvEntriesFactory<SimpleAllocator, 2048, 4096>::create(alloc1);
      {
        auto res = qp->post_recvs(*recv_rs_at_send, 2048);
        RDMA_ASSERT(res == IOCode::Ok);
      }

      UDTransport sender;
      ASSERT(sender.connect(FLAGS_addr, "b" + std::to_string(thread_id),
                            thread_id, qp) == IOCode::Ok)
          << " connect failure at addr: " << FLAGS_addr;

      RPCCore<SendTrait, RecvTrait, SManager> rpc(12);
      auto send_buf = std::get<0>(alloc1.alloc_one(4096).value());
      ASSERT(send_buf != nullptr);
      auto lkey = handler1->get_reg_attr().value().key;

      memset(send_buf, 0, 4096);

      // 0. connect the RPC
      // first we send the connect transport
      auto conn_op = RPCOp::get_connect_op(MemBlock(send_buf, 2048),
                                           sender.get_connect_data().value());
      auto ret = conn_op.execute_w_key(&sender, lkey);
      ASSERT(ret == IOCode::Ok);

      UDRecvTransport<2048> recv_s(qp, recv_rs_at_send);

      SScheduler ssched;
      rpc.reg_poll_future(ssched, &recv_s);

      usize total_processed = 0;

      auto rc = RC::create(nic_for_sender, QPConfig()).value();

      // 2. create the pair QP at server using CM
      ConnectManager cm(FLAGS_addr);
      if (cm.wait_ready(1000000, 2) ==
          IOCode::Timeout) // wait 1 second for server to ready, retry 2 times
        RDMA_ASSERT(false) << "cm connect to server timeout";

      auto qp_res =
          cm.cc_rc(FLAGS_client_name + " thread-qp" + std::to_string(thread_id),
                   rc,nic_idx, QPConfig());
      RDMA_ASSERT(qp_res == IOCode::Ok) << std::get<0>(qp_res.desc);

      auto key = std::get<1>(qp_res.desc);
      //RDMA_LOG(4) << "t-" << thread_id << " fetch QP authentical key: " << key;

      auto fetch_res = cm.fetch_remote_mr(nic_idx);
      RDMA_ASSERT(fetch_res == IOCode::Ok) << std::get<0>(fetch_res.desc);
      rmem::RegAttr remote_attr = std::get<1>(fetch_res.desc);

      rc->bind_remote_mr(remote_attr);
      rc->bind_local_mr(handler1->get_reg_attr().value());

      // 1. bootstrap the XCache
      if (thread_id == 0) {
        char reply_buf[1024];
        // only use thread 0 to bootstrap
        RPCOp op;
        op.set_msg(MemBlock((char *)send_buf + 2048, 2048))
            .set_req()
            .set_rpc_id(META)
            .set_corid(0)
            .add_one_reply(rpc.reply_station,
                           {.mem_ptr = reply_buf, .sz = 1024})
            .add_arg<u64>(73);
        ASSERT(rpc.reply_station.cor_ready(0) == false);
        auto ret = op.execute_w_key(&sender, lkey);
        ASSERT(ret == IOCode::Ok);
        // wait for the reply
        while (rpc.reply_station.cor_ready(0) == false) {
          r2::compile_fence();
          rpc.recv_event_loop(&recv_s);
        }

        // parse the reply
        auto meta =
            ::xstore::util::Marshal<ReplyMeta>::deserialize(reply_buf, 1024);
        LOG(4) << "dispatch num: "
               << " " << meta.dispatcher_sz << "model buf: " << meta.model_buf
               << "; tt_buf: " << meta.tt_buf;

        // try serialize the first layer
        cache = std::make_unique<XCache>(
            std::string(reply_buf + sizeof(ReplyMeta), meta.dispatcher_sz));
        LOG(4) << "first layer check: " << cache->first_layer.up_bound
               << "; num: " << cache->first_layer.dispatch_num;

        // try serialize the second layer
        // we first create the QP to fetch the model
        char *xcache_buf = reinterpret_cast<char *>(
            std::get<0>(alloc1.alloc_one(meta.total_sz).value()));

        {
          AsyncOp<1> op;
          op.set_read()
            .set_payload((const u64 *)xcache_buf, meta.total_sz,
                         rc->local_mr.value().lkey);
          op.set_rdma_rbuf((const u64 *)meta.model_buf, remote_attr.key);

          auto ret = op.execute(rc,IBV_SEND_SIGNALED);
          ASSERT(ret == IOCode::Ok);
          auto res_p = rc->wait_one_comp();
          ASSERT(res_p == IOCode::Ok);
        }

        // finally, deserialize the xcache


        ASSERT(false) << "not impl";
      }

      // 2. wait for the XCache to bootstrap done
      bar.wait();

      for (uint i = 0; i < FLAGS_coros; ++i) {
        ssched.spawn([&statics, &total_processed, &sender, &rpc, lkey, send_buf,
                      thread_id](R2_ASYNC) {
          char reply_buf[1024];

          while (1) {
          }

          LOG(4) << "coros: " << R2_COR_ID() << " exit";

          if (R2_COR_ID() == FLAGS_coros) {

            R2_STOP();
          }
          R2_RET;
        });
      }
      ssched.run();
      LOG(4) << "after run, total processed: " << total_processed
             << " at client: " << thread_id;

      return 0;
    })));
  }

  for (auto &w : workers) {
    w->start();
  }

  Reporter::report_thpt(statics, 10);

  for (auto &w : workers) {
    w->join();
  }

  LOG(4) << "YCSB client finishes";
  return 0;
}
