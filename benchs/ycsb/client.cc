#include <gflags/gflags.h>
#include <type_traits>

#include "../../deps/kvs-workload/static_loader.hh"
#include "../../deps/kvs-workload/ycsb/mod.hh"
using namespace kvs_workloads::ycsb;

#include "./proto.hh"

#include "../../xcomm/tests/transport_util.hh"

#include "../../xutils/file_loader.hh"
#include "../../xutils/local_barrier.hh"
#include "../../xutils/marshal.hh"

#include "../../deps/r2/src/thread.hh"

#include "../../benchs/reporter.hh"

#include "./eval_c.hh"

using namespace test;

using namespace xstore;
using namespace xstore::rpc;
using namespace xstore::transport;
using namespace xstore::bench;
using namespace xstore::util;

// flags
DEFINE_int64(threads, 1, "num client thread used");
DEFINE_int64(coros, 1, "num client coroutine used per threads");
DEFINE_string(addr, "localhost:8888", "server address");

DEFINE_bool(vlen, false, "whether to use variable length value");

DEFINE_uint64(nkeys, 1000000, "Number of keys to fetch");

DEFINE_bool(load_from_file, true, "whether to load DB from the file");
DEFINE_string(data_file, "lognormal_uni_100m.txt", "data file name");

using XThread = ::r2::Thread<usize>;

namespace xstore {
std::unique_ptr<XCache> cache = nullptr;
std::vector<XCacheTT> tts;
} // namespace xstore

std::shared_ptr<std::vector<u64>> all_keys =
  std::make_shared<std::vector<u64>>();

volatile bool running = true;

int
main(int argc, char** argv)
{

  gflags::ParseCommandLineFlags(&argc, &argv, true);

  std::vector<std::unique_ptr<XThread>> workers;

  std::vector<::xstore::bench::Statics> statics(FLAGS_threads);

  if (FLAGS_load_from_file) {
    FileLoader loader(FLAGS_data_file);

    for (usize i = 0; i < FLAGS_nkeys; ++i) {
      auto key = loader.next_key<u64>(FileLoader::default_converter<u64>);
      if (key) {
        all_keys->push_back(key.value());
      } else {
        break;
      }
    }
  }

  PBarrier bar(FLAGS_threads + 1);

  for (uint thread_id = 0; thread_id < FLAGS_threads; ++thread_id) {
    workers.push_back(std::move(
      std::make_unique<XThread>([&statics, &bar, thread_id]() -> usize {
        usize nic_idx = 0;
        if (thread_id >= 12) {
          nic_idx = 1;
        }
        auto nic_for_sender =
          RNic::create(RNicInfo::query_dev_names().at(nic_idx)).value();
        auto qp = UD::create(nic_for_sender, QPConfig()).value();

        auto mem_region1 = HugeRegion::create(128 * 1024 * 1024).value();
        if (thread_id == 0) {
          // thread 0 uses a larger region
          mem_region1 = HugeRegion::create(512 * 1024 * 1024).value();
        }
        auto mem1 = mem_region1->convert_to_rmem().value();
        auto handler1 = RegHandler::create(mem1, nic_for_sender).value();
        SimpleAllocator alloc1(mem1, handler1->get_reg_attr().value());
        auto recv_rs_at_send =
          RecvEntriesFactory<SimpleAllocator, 2048, 1024>::create(alloc1);
        {
          auto res = qp->post_recvs(*recv_rs_at_send, 2048);
          RDMA_ASSERT(res == IOCode::Ok);
        }

        auto id = 1024 * FLAGS_client_name + thread_id;
        UDTransport sender;
        {
          r2::Timer t;
          do {
            auto res = sender.connect(
              FLAGS_addr, "b" + std::to_string(thread_id), id, qp);
            if (res == IOCode::Ok) {
              break;
            }
            if (t.passed_sec() >= 10) {
              ASSERT(false) << "conn failed at thread:" << thread_id;
            }
          } while (t.passed_sec() < 10);
        }

        RPCCore<SendTrait, RecvTrait, SManager> rpc(12);
        auto send_buf = std::get<0>(alloc1.alloc_one(1024).value());
        ASSERT(send_buf != nullptr);
        auto lkey = handler1->get_reg_attr().value().key;

        memset(send_buf, 0, 1024);

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
            IOCode::Timeout) // wait 1 second for server to ready, retry 2
                             // times
          RDMA_ASSERT(false) << "cm connect to server timeout";

        auto qp_res =
          cm.cc_rc(FLAGS_client_name + " thread-qp" + std::to_string(thread_id),
                   rc,
                   nic_idx,
                   QPConfig());
        RDMA_ASSERT(qp_res == IOCode::Ok) << std::get<0>(qp_res.desc);

        auto key = std::get<1>(qp_res.desc);
        // RDMA_LOG(4) << "t-" << thread_id << " fetch QP authentical key: "
        // << key;

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
          op.set_msg(MemBlock((char*)send_buf + 2048, 2048))
            .set_req()
            .set_rpc_id(META)
            .set_corid(0)
            .add_one_reply(rpc.reply_station,
                           { .mem_ptr = reply_buf, .sz = 1024 })
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
            std::string(reply_buf + sizeof(ReplyMeta), meta.dispatcher_sz),
            false);
          LOG(4) << "first layer check: " << cache->first_layer.up_bound
                 << "; num: " << cache->first_layer.dispatch_num
                 << " total sz:" << meta.total_sz;

          // try serialize the second layer
          // we first create the QP to fetch the model
          char* xcache_buf = reinterpret_cast<char*>(
            std::get<0>(alloc1.alloc_one(meta.total_sz).value()));

          {
            AsyncOp<1> op;
            op.set_read().set_payload(
              (const u64*)xcache_buf, meta.total_sz, rc->local_mr.value().lkey);
            op.set_rdma_rbuf((const u64*)meta.model_buf, remote_attr.key);

            auto ret = op.execute(rc, IBV_SEND_SIGNALED);
            ASSERT(ret == IOCode::Ok);
            auto res_p = rc->wait_one_comp();
            ASSERT(res_p == IOCode::Ok);

            char* cur_ptr = xcache_buf;
            // first we parse the model
            XCache::Sub sub;
            const usize submodel_sz = sub.serialize().size();

            ASSERT(cache->second_layer.size() == 0);
            for (uint i = 0; i < cache->first_layer.dispatch_num; ++i) {
              // cache->second_layer.emplace_back(std::make_shared<>(std::string(cur_ptr,
              // submodel_sz)));
              cache->emplace_one_second_model(
                ::xstore::string_view(cur_ptr, submodel_sz));
              cur_ptr += submodel_sz;
            }

            LOG(4) << "serialzie second layer done:" << cur_ptr - xcache_buf
                   << ":" << meta.total_sz;

            // then the TT
            tts.reserve(cache->first_layer.dispatch_num);
            LOG(4) << "check dispatch num: " << cache->first_layer.dispatch_num;
            for (uint i = 0; i < cache->first_layer.dispatch_num; ++i) {
              ASSERT(cur_ptr >= xcache_buf);
              if (i % 100 == 0) {
                // LOG(4) << "load :" << i << " done; total: " <<
                // cache->first_layer.dispatch_num;
              }

              auto tt_n =
                ::xstore::util::Marshal<u32>::deserialize(cur_ptr, sizeof(u64));
              const ::xstore::string_view buf((const char*)cur_ptr,
                                              sizeof(u32) +
                                                tt_n * sizeof(XCacheTT::ET));
              // serialize one
              tts.emplace_back(buf);
              cur_ptr += tts[i].tt_sz();
            }

            LOG(4) << "serialzie TTs done:" << cur_ptr - xcache_buf << ":"
                   << meta.total_sz;
          }
        }

        // 2. wait for the XCache to bootstrap done
        bar.wait();

        YCSBCWorkloadUniform ycsb(
          FLAGS_nkeys, 0xdeadbeaf + thread_id + FLAGS_client_name * 73);
        ::kvs_workloads::StaticLoader other(
          all_keys, 0xdeadbeaf + thread_id + FLAGS_client_name * 73);

        worker_id = thread_id;

        for (uint i = 0; i < FLAGS_coros; ++i) {
          ssched.spawn([&statics,
                        &total_processed,
                        &sender,
                        &rc,
                        &alloc1,
                        &ycsb,
                        &other,
                        &rpc,
                        lkey,
                        send_buf,
                        thread_id](R2_ASYNC) {
            char reply_buf[1024];
            char* my_buf = reinterpret_cast<char*>(
              std::get<0>(alloc1.alloc_one(8192).value()));

            while (running) {
              r2::compile_fence();
              // const auto key = XKey(ycsb.next_key());
              XKey key;
              if (FLAGS_load_from_file) {
                key = XKey(other.next_key());
              } else {
                key = XKey(ycsb.next_key());
              }

              if (!FLAGS_vlen) {
                auto res = core_eval(key,
                                     rc,
                                     rpc,
                                     sender,
                                     my_buf,
                                     statics[thread_id],
                                     R2_ASYNC_WAIT);
                if (std::is_integral<ValType>::value) {
                  // check the value if it is a integer
                  ASSERT(XKey(res) == key) << XKey(res) << "; target:" << key;
                }
              } else {
                auto res = core_eval_v(key, rc, my_buf, R2_ASYNC_WAIT);
                // TODO: how to check the value's correctness?
              }
              statics[thread_id].increment();
            }

            // LOG(4) << "coros: " << R2_COR_ID() << " exit";

            if (R2_COR_ID() == FLAGS_coros) {
              R2_STOP();
            }
            R2_RET;
          });
        }
        ssched.run();
        if (thread_id == 0) {
          LOG(4) << "after run, total processed: " << total_processed
                 << " at client: " << thread_id;
        }

        if (FLAGS_client_name == 1 && worker_id == 0) {
          error_cdf.finalize();
          LOG(4) << "Error data: "
                 << "[average: " << error_cdf.others.average
                 << ", min: " << error_cdf.others.min
                 << ", max: " << error_cdf.others.max;
          LOG(4) << error_cdf.dump_as_np_data() << "\n";
        }

        return 0;
      })));
  }

  for (auto& w : workers) {
    w->start();
  }

  bar.wait();
  Reporter::report_thpt(
    statics, 20, std::to_string(FLAGS_client_name) + ".xstorelog");
  running = false;

  for (auto& w : workers) {
    w->join();
  }

  LOG(4) << "YCSB client finishes";
  return 0;
}
