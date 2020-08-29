#include <gflags/gflags.h>

#include "../tests/transport_util.hh"

#include "../src/rpc/mod.hh"
#include "../src/transport/rdma_ud_t.hh"

#include "../../deps/r2/src/thread.hh"

#include "../../benchs/reporter.hh"

namespace bench {

using namespace test;

using namespace xstore::rpc;
using namespace xstore::transport;

// prepare the sender transport
using SendTrait = UDTransport;
using RecvTrait = UDRecvTransport<2048>;
using SManager = UDSessionManager<2048>;

} // namespace bench

using namespace bench;
using namespace xstore::bench;

DEFINE_int64(threads, 1, "num client thread used");
DEFINE_int64(coros, 1, "num client coroutine used per threads");
DEFINE_string(addr, "localhost:8888", "server address");

using XThread = ::r2::Thread<usize>;

int main(int argc, char **argv) {

  gflags::ParseCommandLineFlags(&argc, &argv, true);

  std::vector<std::unique_ptr<XThread>> workers;

  std::vector<Statics> statics(FLAGS_threads);

  for (uint thread_id = 0; thread_id < FLAGS_threads; ++thread_id) {
    workers.push_back(
        std::move(std::make_unique<XThread>([&statics, thread_id]() -> usize {
          auto nic_for_sender =
              RNic::create(RNicInfo::query_dev_names().at(0)).value();
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
          // first we send the connect transport
          auto conn_op = RPCOp::get_connect_op(
              MemBlock(send_buf, 2048), sender.get_connect_data().value());
          auto ret = conn_op.execute_w_key(&sender, lkey);
          ASSERT(ret == IOCode::Ok);

          UDRecvTransport<2048> recv_s(qp, recv_rs_at_send);

          SScheduler ssched;
          rpc.reg_poll_future(ssched, &recv_s);

          usize total_processed = 0;

          // TODO: spawn coroutines for sending the reqs
          for (uint i = 0; i < FLAGS_coros; ++i) {
            ssched.spawn([&statics, &total_processed, &sender, &rpc, lkey,
                          send_buf, thread_id](R2_ASYNC) {
              char reply_buf[1024];

              while (1) {
                RPCOp op;
                op.set_msg(MemBlock((char *)send_buf + 2048, 2048))
                    .set_req()
                    .set_rpc_id(0)
                    .set_corid(R2_COR_ID())
                    .add_one_reply(rpc.reply_station,
                                   {.mem_ptr = reply_buf, .sz = 1024})
                    .add_arg<u64>(73);
                ASSERT(rpc.reply_station.cor_ready(R2_COR_ID()) == false);
                auto ret = op.execute_w_key(&sender, lkey);
                ASSERT(ret == IOCode::Ok);

                // yield the coroutine to wait for reply
                R2_PAUSE_AND_YIELD;
                // the reply is done
                // ASSERT(false) << "recv rpc reply: " << *((u64 *)reply_buf);
                total_processed += 1;
                statics[thread_id].increment();
                if (total_processed > 10000000) {
                  break;
                }
              }

              LOG(4) << "coros: " << R2_COR_ID() << " exit";

              if (R2_COR_ID() == FLAGS_coros) {
                // send an RPC to stop the server
                RPCOp op;
                op.set_msg(MemBlock((char *)send_buf + 2048, 2048))
                    .set_req()
                    .set_rpc_id(1) // stop
                    .set_corid(R2_COR_ID());
                auto ret = op.execute_w_key(&sender, lkey);
                ASSERT(ret == IOCode::Ok);

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

  LOG(4) << "rpc client finishes";
  return 0;
}
