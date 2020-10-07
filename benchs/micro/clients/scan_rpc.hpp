#pragma once

#include "mem_region.hpp"

//#include "../net_config.hpp"
#include "../statics.hpp"
#include "lib.hpp"

#include "../../../server/proto.hpp"

#include "../../../cli/lib.hpp"
#include "data_sources/ycsb/workloads.hpp"

namespace fstore {

using namespace sources::ycsb;

namespace bench {

RdmaCtrl&
global_rdma_ctrl();
RegionManager&
global_memory_region();

extern volatile bool running;

class RPCScanClient
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
    LOG(4) << "[RPC get workload (YCSB-C) client] #" << my_id << " bootstrap "
           << num_threads << " threads.";
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
        ASSERT(nic.ready());

        // First we register the memory
        auto ret = global_rdma_ctrl().mr_factory.register_mr(
          thread_id,
          global_memory_region().base_mem_,
          global_memory_region().base_size_,
          nic);
        ASSERT(ret == SUCC) << "failed to register memory region.";
        LOG(4) << "client create adapter";
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
                                thread_id) != SUCC) {
        }

        LOG(4) << "null client #" << thread_id << " bootstrap done, "
               << " will create: [" << concurrency
               << "] coroutines for execution.";

        u64 thread_random_seed = FLAGS_seed + 73 * thread_id;
        /**
         * Start the main loop
         */
        RScheduler r;
        RPC rpc(adapter);

        rpc.spawn_recv(r);

        bar.wait();

        r.spawnr([&](handler_t& h, RScheduler& r) {
          auto id = r.cur_id();

          auto ret = rpc.start_handshake(addr, r, h);
          ASSERT(ret == SUCC) << "start handshake error: " << ret;

          for (uint i = 0; i < concurrency; ++i) {
            r.spawnr([&](handler_t& h, RScheduler& r) {
              auto id = r.cur_id();

              auto& factory = rpc.get_buf_factory();
              auto send_buf = factory.alloc(128);

              /**
               * Because it is get workload, we use YCSB-C, which is a read-only
               * workloads
               */
              // YCSBCWorkload workload(FLAGS_total_accts,thread_random_seed +
              // id * 0xddd + 0xaaa,FLAGS_need_hash);
              YCSBScanWorkload workload(FLAGS_max_key,
                                        thread_random_seed + id * 0xddd + 0xaaa,
                                        FLAGS_need_hash);
              u64 sum = 0;

              Timer t;
              FlatLatRecorder lats;

              while (running) {
                char reply_buf[4096];
                auto range = workload.next_range();
                ScanPayload req = { .table_id = 0,
                                    .start = std::get<0>(range),
                                    .num = std::get<1>(range) };
                Marshal<ScanPayload>::serialize_to(req, send_buf);
                ASSERT(req.num > 0);

                auto ret = rpc.call({ .cor_id = id, .dest = addr },
                                    SCAN_RPC,
                                    { .send_buf = send_buf,
                                      .len = sizeof(ScanPayload),
                                      .reply_buf = reply_buf,
                                      .reply_cnt = 1 });
                ASSERT(ret == SUCC);
                r.pause_and_yield(h);
                auto val = *((u64*)reply_buf);
                sum += val;

                if (thread_id == 0 && id == 2) {
                  lats.add_one(t.passed_msec());
                  t.reset();
                  statics[thread_id].data.lat = lats.get_lat();
                }

                statics[thread_id].increment();
                r.yield_to_next(h);
              }
              if (r.cur_id() == 1)
                LOG(4) << sum;
              r.stop_schedule();
              routine_ret(h, r);
            });
          }
          // stop the bootstrap coroutine
          routine_ret(h, r);
        });

        r.run();
        rpc.end_handshake(addr);
        return 0;
      }));
    }
    return handlers;
  }
};

} // namespace bench

} // namespace fstore
