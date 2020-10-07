#pragma once

#include "mem_region.hpp"
#include "thread.hpp"

//#include "../net_config.hpp"
#include "../../arc/val/cpu.hh"

#include "../statics.hpp"
#include "lib.hpp"

#include "../../../server/proto.hpp"

namespace fstore {

  using namespace platforms;

namespace bench {

RdmaCtrl&
global_rdma_ctrl();
RegionManager&
global_memory_region();

extern volatile bool running;

using Worker = Thread<double>;

class NullClient
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
    LOG(4) << "client #" << my_id << " bootstrap " << num_threads
           << " threads.";
    std::vector<Worker*> handlers;
    for (uint thread_id = 0; thread_id < num_threads; ++thread_id) {
      handlers.push_back(new Worker([&,
                                     thread_id,
                                     my_id,
                                     server_addr,
                                     concurrency,
                                     server_port,
                                     server_host]() {
                                      //if (FLAGS_bind_core)
                                      //CoreBinder::bind(thread_id);
                                      VALBinder::bind( thread_id / VALBinder::core_per_socket(),
                                                       thread_id % VALBinder::core_per_socket());

        Addr addr({ .mac_id = server_addr.mac_id, .thread_id = thread_id });

        auto all_devices = RNicInfo::query_dev_names();
        ASSERT(!all_devices.empty()) << "RDMA must be supported.";

        auto nic_id = VALNic::choose_nic(thread_id);
        //auto nic_id = 0;
        ASSERT(nic_id < all_devices.size()) << "wrong dev id:" << nic_id;
        RNic nic(all_devices[VALNic::choose_nic(thread_id)]);
        // First we register the memory
        auto ret = global_rdma_ctrl().mr_factory.register_mr(
          thread_id,
          global_memory_region().base_mem_,
          global_memory_region().base_size_,
          nic);
        ASSERT(ret == SUCC) << "failed to register memory region.";

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
        LOG(4) << "start to connect "
               << "@" << thread_id;
        while (adapter->connect(addr,
                                ::rdmaio::make_id(server_host, server_port),
                                thread_id) != SUCC) {
          sleep(1);
        }

        LOG(4) << "null client #" << thread_id << " bootstrap done, "
               << " will create: [" << concurrency
               << "] coroutines for execution.";
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
          if (thread_id == 0)
            LOG(4) << "start handshake done";

          for (uint i = 0; i < concurrency; ++i) {
            r.spawnr([&](handler_t& h, RScheduler& r) {
              auto id = r.cur_id();
              auto& factory = rpc.get_buf_factory();
              auto send_buf = factory.alloc(128);

              while (running) {
                r2::compile_fence();
                char reply_buf[32];
                auto ret = rpc.call({ .cor_id = id, .dest = addr },
                                    ::fstore::server::NULL_ID,
                                    { .send_buf = send_buf,
                                      .len = sizeof(u64),
                                      .reply_buf = reply_buf,
                                      .reply_cnt = 1 });
                ASSERT(ret == SUCC);
                r.pause_and_yield(h);
                statics[thread_id].increment();
                // yield for others
                r.yield_to_next(h);
              }
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
