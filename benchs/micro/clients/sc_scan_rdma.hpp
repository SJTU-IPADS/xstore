#pragma once

#include "mem_region.hpp"
#include "thread.hpp"

//#include "../net_config.hpp"
#include "../statics.hpp"
#include "lib.hpp"

#include "r2/src/futures/rdma_future.hpp"
#include "r2/src/random.hpp"

#include "utils/cc_util.hpp"

#include "../../../cli/lib.hpp"
#include "data_sources/ycsb/scan_workloads.hpp"

#define HYBRID 0 // use RDMA + RPC

namespace fstore {

using namespace server;
using namespace r2;
using namespace r2::util;
using namespace sources::ycsb;

namespace bench {

RdmaCtrl&
global_rdma_ctrl();
RegionManager&
global_memory_region();

extern volatile bool running;
extern SC* sc;

using Worker = Thread<double>;

DEFINE_uint64(max_key, 4294967109L, "Maximum of key-space to search.");

/*!
  Evaluate the performance of scan using RDMA
 */
class SCRDMASClient
{
public:
  static std::vector<Worker*> bootstrap_all(std::vector<Statics>& statics,
                                            const Addr& server_addr,
                                            const std::string& server_host,
                                            const u64& server_port,
                                            PBarrier& bar,
                                            u64 num_threads,
                                            u64 my_id,
                                            u64 concurrency = 1)
  {
    std::vector<Worker*> handlers;

    for (uint thread_id = 0; thread_id < num_threads; ++thread_id) {
      handlers.push_back(new Worker([&,
                                     thread_id,
                                     my_id,
                                     server_addr,
                                     concurrency,
                                     server_port,
                                     server_host]() {
        Addr addr({ .mac_id = server_addr.mac_id, .thread_id = thread_id });

        auto all_devices = RNicInfo::query_dev_names();
        ASSERT(!all_devices.empty()) << "RDMA must be supported.";

        auto nic_id = VALNic::choose_nic(thread_id);
        ASSERT(nic_id < all_devices.size()) << "wrong dev id:" << nic_id;
        RNic nic(all_devices[VALNic::choose_nic(thread_id)]);
        // First we register the memory
        auto ret = global_rdma_ctrl().mr_factory.register_mr(
          thread_id,
          global_memory_region().base_mem_,
          global_memory_region().base_size_,
          nic);
        ASSERT(ret == SUCC) << "failed to register memory region.";

        // u64 remote_connect_id = 0;
        u64 remote_connect_id = thread_id;
        auto adapter =
          Helper::create_thread_ud_adapter(global_rdma_ctrl().mr_factory,
                                           global_rdma_ctrl().qp_factory,
                                           nic,
                                           my_id,
                                           thread_id);
        ASSERT(adapter != nullptr) << "failed to create UDAdapter";
        /**
         * connect to the server peer, notice that we use the
         * **thread_id** as the QP_id.
         */
        while (adapter->connect(addr,
                                ::rdmaio::make_id(server_host, server_port),
                                remote_connect_id) != SUCC) {
          sleep(1);
        }

        /**
         * Fetch server's MR
         */
        RemoteMemory::Attr remote_mr;
        while (RMemoryFactory::fetch_remote_mr(
                 remote_connect_id,
                 ::rdmaio::make_id(server_host, server_port),
                 remote_mr) != SUCC) {
        }

        LOG(4) << "null client #" << thread_id << " bootstrap done, "
               << " will create: [" << concurrency
               << "] coroutines for execution.";

        u64 thread_random_seed = FLAGS_seed + 73 * thread_id;

        RScheduler r;
        RPC rpc(adapter);
        rpc.spawn_recv(r);

        u64 qp_id = my_id << 32 | (thread_id + 1);

        r.spawnr([&](handler_t& h, RScheduler& r) {
          auto id = r.cur_id();

          auto ret = rpc.start_handshake(addr, r, h);
          ASSERT(ret == SUCC) << "start handshake error: " << ret;

          // then we create the specificed QP
          auto qp = Helper::create_connect_qp(global_rdma_ctrl().mr_factory,
                                              global_rdma_ctrl().qp_factory,
                                              nic,
                                              qp_id,
                                              thread_id,
                                              remote_mr,
                                              addr,
                                              rpc,
                                              r,
                                              h);
          ASSERT(qp != nullptr);
          LOG(4) << "create connected qp done";

          /**
           * Only fetch the SC from remote at the first thread.
           * This saves the memory used, since the SC can be very large.
           */
          if (thread_id == 0)
            sc = ModelFetcher::bootstrap_remote_sc(0, rpc, addr, qp, r, h);

          auto server_meta = Helper::fetch_server_meta(addr, rpc, r, h);

          /*
            The next code block verfies that our local fetched model sync with
            the remote model. It can be costly if there are many keys to verify.
          */
#if 1
          if (thread_id == 0) {
            u64 max_page_span = 0;
            u64 min_page_span = 10240;

            // sanity check all the predicts
            YCSBHashGenereator it(0, FLAGS_total_accts);
            auto& factory = rpc.get_buf_factory();
            auto send_buf = factory.alloc(128);

            char reply_buf[1024];
            Timer t;
            CDF<u64> page_span_cdf("page");

            for (it.begin(); it.valid(); it.next()) {
#if 0
                                      GetPayload req = {.table_id = 0,.key = it.key() };
                                      Marshal<GetPayload>::serialize_to(req,send_buf);
                                      auto ret = rpc.call({.cor_id = id,.dest = addr},
                                                          PREDICT,
                                                          {.send_buf = send_buf,.len = sizeof(GetPayload),
                                                           .reply_buf = reply_buf,.reply_cnt = 1});
                                      r.pause_and_yield(h);
                                      auto predict = *((Predicts *)reply_buf);
                                      auto predict_local = sc->get_predict(it.key());
                                      ASSERT(predict.start == predict_local.start) << "predict.start: " << predict.start << ";"
                                                                                   << "local start: " << predict_local.start << ";"
                                                                                   << "for key: " << it.key() << "@thread: " << thread_id;
                                      ASSERT(predict.pos == predict_local.pos);
                                      ASSERT(predict.end == predict_local.end);
#endif

              // max_page_span = std::max(static_cast<u64>(predict.end -
              // predict.start + 1),max_page_span);
              auto predict = sc->get_predict(it.key());
              auto page_span = sc->mp->decode_mega_to_entry(predict.end) -
                               sc->mp->decode_mega_to_entry(predict.start) + 1;
              min_page_span =
                std::min(static_cast<u64>(page_span), min_page_span);
              max_page_span =
                std::max(static_cast<u64>(page_span), max_page_span);
              page_span_cdf.insert(page_span);
            }
            LOG(4) << "all predicts verify done, uses " << t.passed_msec()
                   << " msec, max page span: " << max_page_span << " over "
                   << FLAGS_total_accts;
            page_span_cdf.finalize();
            FILE_WRITE("span.py", std::ofstream::out)
              << page_span_cdf.dump_as_np_data();
          }
#endif
          LOG(4) << "check max key: " << FLAGS_max_key;

          // spawn coroutines for executing RDMA reqs
          for (uint i = 0; i < concurrency; ++i) {
            r.spawnr([&, server_meta](R2_ASYNC) {
              auto id = R2_COR_ID();

              char* local_buf = rpc.get_buf_factory().alloc(409600);
              FastRandom rand2(thread_random_seed + id * 0xccc + 0xbbb);

              /**
               * Because it is get workload, we use YCSB-C, which is a read-only
               * workloads
               */
              // YCSBCWorkloadUniform workload(FLAGS_total_accts,
              //                            thread_random_seed + id * 0xddd +
              //                            0xaaa,FLAGS_need_hash);
              YCSBScanWorkload workload(FLAGS_max_key,
                                        thread_random_seed + id * 0xddd + 0xaaa,
                                        FLAGS_need_hash);

              FClientScan fc(
                qp, sc, server_meta.page_addr, server_meta.page_area_sz);
              u64 sum = 0;

              Timer t;
              FlatLatRecorder lats;

              while (running) {
                auto range = workload.next_range();
                auto ret = fc.seek(std::get<0>(range),
                                   local_buf,
                                   std::get<1>(range),
                                   R2_ASYNC_WAIT);
                if (ret) {
                  sum += ret.value();
                  u64 estimated_pages = std::get<1>(range) / IM * 2;
                  fc.fetch_value_pages(
                    std::make_pair(ret.value(), ret.value() + estimated_pages),
                    local_buf,
                    R2_ASYNC_WAIT);
                  sum += *((u64*)local_buf);
                } else {
                  ASSERT(false);
                }
                if (thread_id == 0 && id == 2) {
                  lats.add_one(t.passed_msec());
                  t.reset();
                  statics[thread_id].data.lat = lats.get_lat();
                }
                statics[thread_id].increment();
                R2_YIELD;
              }
              /**
               * just avoid compiler from optimizing this out
               */
              if (thread_id == 0 && r.cur_id() == 73)
                LOG(4) << "get sum: " << sum;
              R2_STOP();
              R2_RET;
            });
          }

          bar.wait();
          ASSERT(sc != nullptr);
          routine_ret(h, r);
        });
        r.run();

        Helper::send_rc_deconnect(rpc, addr, qp_id);
        rpc.end_handshake(addr);
        return 0;
      }));
    }
    return handlers;
  }
};

} // namespace bench

} // namespace fstore
