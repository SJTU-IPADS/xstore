#pragma once

#include "mem_region.hpp"
#include "thread.hpp"

//#include "../net_config.hpp"
#include "../statics.hpp"
#include "lib.hpp"

#include "r2/src/futures/rdma_future.hpp"
#include "r2/src/random.hpp"
#include "r2/src/rdma/single_op.hpp"

#include "utils/cc_util.hpp"
#include "utils/data_map.hpp"

#include "../../../cli/lib.hpp"
#include "data_sources/ycsb/stream.hpp"

namespace fstore {

namespace bench {

using namespace server;
using namespace r2;
using namespace r2::util;
using namespace sources::ycsb;
using namespace utils;

RdmaCtrl&
global_rdma_ctrl();
RegionManager&
global_memory_region();

extern volatile bool running;

using Worker = Thread<double>;

DEFINE_bool(my_insert, false, "Whether use my customed insertion for work.");
DEFINE_int64(num, 10000000, "Num of accounts loaded for YCSB.");
DEFINE_int64(pre_load, 100000, "Number of preload data for dynamic workload.");

// the key space we should update
u64 max_key = 0;

class DynamicClient
{
public:
  static std::vector<Worker*> bootstrap(std::vector<Statics>& statics,
                                        const Addr& server_addr,
                                        const std::string& server_host,
                                        const u64& server_port,
                                        PBarrier& bar,
                                        u64 num_threads,
                                        u64 my_id,
                                        u64 concurrency = 1)
  {
    LOG(4) << "client use dynamic changing workloads";
    std::vector<Worker*> handlers;

    {
      //          YCSBGenerator it(0, FLAGS_num, 0xdeadbeaf);
      YCSBHashGenereator it(0, FLAGS_num);

      // first we calculate the max key
      for (it.begin(); it.valid(); it.next())
        max_key = std::max(max_key, it.key());
    }

    // first we spawn a threads to insert
    handlers.push_back(new Worker(
      [&, my_id, server_addr, concurrency, server_port, server_host] {
        // client id start at 1. we only use one client thread for insertion
        if (FLAGS_my_insert == false || my_id != 1) {
          LOG(2) << "insert thread do nothing, just exit";
          bar.wait();
          return 0;
        }

        usize thread_id = 0;
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

        u64 remote_connect_id = 0;
        auto adapter =
          Helper::create_thread_ud_adapter(global_rdma_ctrl().mr_factory,
                                           global_rdma_ctrl().qp_factory,
                                           nic,
                                           my_id,
                                           thread_id);
        ASSERT(adapter != nullptr) << "failed to create UDAdapter";
        LOG(2) << "dynamic insert start to create rpc";
        /**
         * connect to the server peer, notice that we use the
         * **thread_id** as the QP_id.
         */
        IOStatus res;
        while (
          (res = adapter->connect(addr,
                                  ::rdmaio::make_id(server_host, server_port),
                                  remote_connect_id)) != SUCC) {
          sleep(1);
        }

        bar.wait();

        RScheduler r;
        RPC rpc(adapter);
        rpc.spawn_recv(r);

        r.spawnr([&](R2_ASYNC) {
          auto ret = rpc.start_handshake(addr, R2_ASYNC_WAIT2);
          ASSERT(ret == SUCC) << "start handshake error: " << ret;

          LOG(2) << "insert thread started!";
          char reply_buf[128];
          auto buf_factory = rpc.get_buf_factory();
          auto send_buf = buf_factory.alloc(4096);

          LOG(4) << "client start populating " << FLAGS_num << " tuples";

          YCSBHashGenereator it(0, FLAGS_num);

          u64 counter = 0;
          u64 prev_key = 0;
          std::vector<u64> key_span; // record all inserted keys

          Timer timer;

          /*
            I've verified that the keys are uniformally distributed across
            [0,max_key], so there are frequently key splits
          */
          for (it.begin(); it.valid(); it.next(), counter += 1) {
            // prepare the workload req
            using PutPayload =
              GetPayload; // put uses the same header's argument as get
            PutPayload req = { .table_id = 0, .key = it.key() };

            Marshal<PutPayload>::serialize_to(req, send_buf);

            //
            auto ret = rpc.call({ .cor_id = R2_COR_ID(), .dest = addr },
                                INSERT_ID,
                                { .send_buf = send_buf,
                                  .len = sizeof(PutPayload) + sizeof(ValType),
                                  .reply_buf = reply_buf,
                                  .reply_cnt = 1 });
            ASSERT(ret == SUCC) << "get err ret call: " << ret;

            //              R2_PAUSE_AND_YIELD;
            R2_EXECUTOR.wait_for(100000000L);
            auto res = R2_PAUSE;
            ASSERT(res == SUCC)
              << "r2 wait res: " << res << "; total: " << counter << " loaded";

            // statics[0].increment();
            // do we need to sanity check the results?

            if (counter == FLAGS_pre_load) {

              DataMap<u64, double> records("dynamic");
              // sample the cur keys to output
              for (uint i = 0; i < key_span.size(); ++i) {
                double res = static_cast<double>(key_span[i]) /
                             static_cast<double>(max_key);
                records.insert(i, res);
              }
              FILE_LOG("dynamic.res") << records.dump_as_np_data();

              // init-load 10000 data
              LOG(4) << "pre load done";
            }

            // update key status
            if (counter < FLAGS_pre_load)
              key_span.push_back(it.key());
          }
          LOG(4) << "client uses " << timer.passed_sec() << " for loading.";

          R2_RET;
        });

        r.run();
        return 0;
      }));

    // spawn worker thread for RPC
    for (usize thread_id = 1; thread_id < num_threads - 1; thread_id += 1) {
      // pass
      // spawn a polling thread for update sc
      handlers.push_back(new Worker([&,
                                     thread_id,
                                     my_id,
                                     server_addr,
                                     concurrency,
                                     server_port,
                                     server_host] {
        // first we build RDMA connections
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
        ASSERT(ret == SUCC)
          << "failed to register memory region for thread: " << thread_id;

        u64 remote_connect_id = 0;
        // u64 remote_connect_id = thread_id;
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

        LOG(4) << "Dynamic client #" << thread_id << " bootstrap done, "
               << " will create: [" << concurrency
               << "] coroutines for execution.";

        u64 thread_random_seed = FLAGS_seed + 73 * thread_id;

        RScheduler r;
        RPC rpc(adapter);
        rpc.spawn_recv(r);

        u64 qp_id = my_id << 32 | (thread_id + 1);

        r.spawnr([&](R2_ASYNC) {
          auto id = R2_COR_ID();

          auto ret = rpc.start_handshake(addr, R2_ASYNC_WAIT2);
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
                                              R2_ASYNC_WAIT2);
          ASSERT(qp != nullptr);
          LOG(4) << "create connected qp done";

          // pass
          bar.wait();
          ASSERT(sc !=
                 nullptr); // if we started, we must initilized the smart cache
          auto server_meta =
            Helper::fetch_server_meta(addr, rpc, R2_ASYNC_WAIT2);

          // TODO: spawn real coroutine functions
          for (uint i = 0; i < concurrency; ++i) {
            r.spawnr([&, server_meta](R2_ASYNC) {
              FastRandom rand(0xdeadbeaf + 0xddd * R2_COR_ID() +
                              thread_id * 73);

              auto local_buf =
                (char*)AllocatorMaster<>::get_thread_allocator()->alloc(40960);
              ASSERT(local_buf != nullptr);

              ASSERT(max_key != 0) << "max key not initialized!";
              while (running) {

                u64 key = rand.next() % max_key;

                // has to be updated locally because sc can be changed due to
                // update threads
                FClient fc(
                  qp, sc, server_meta.page_addr, server_meta.page_area_sz);

                auto predict = fc.get_predict(key);
                auto res = fc.get_addr(key, predict, local_buf, R2_ASYNC_WAIT2);

                switch (std::get<0>(res)) {
                  case SearchCode::Ok:
                    break;
                  case SearchCode::Fallback:
                  case SearchCode::Invalid: {

                    GetPayload req = { .table_id = 0, .key = key };
                    char* send_buf =
                      local_buf + 64; // add a padding to store rpc header

                    Marshal<GetPayload>::serialize_to(req, send_buf);

                    char reply_buf[512];
                    auto ret = rpc.call({ .cor_id = R2_COR_ID(), .dest = addr },
                                        GET_ID,
                                        { .send_buf = send_buf,
                                          .len = sizeof(GetPayload),
                                          .reply_buf = reply_buf,
                                          .reply_cnt = 1 });

                    R2_EXECUTOR.wait_for(1000000000L);
                    auto res = R2_PAUSE;
                    ASSERT(res == SUCC)
                      << "r2 wait res: " << res << " in get() fallback path";

                    // collecting the results
                    // ...
                  } break;
                  case SearchCode::None: {
                    // not exist
                    // we fake a dummy reads here, otherwise it's unfair to
                    // compare SC to RPC. this is because SC uses two round
                    // trip for one get(). if the first try not get the
                    // record, then it will not issue the second read in
                    // get_addr.
                    u64 addr = rand.next() % server_meta.page_area_sz;
                    ::r2::rdma::SROp op(qp);

                    op.set_payload(local_buf, sizeof(ValType))
                      .set_remote_addr(addr)
                      .set_read();
                    auto ret = op.execute(IBV_SEND_SIGNALED, R2_ASYNC_WAIT);
                    ASSERT(std::get<0>(ret) == SUCC);
                  } break;
                  default:
                    ASSERT(false)
                      << "invalid search code returned: " << std::get<0>(res);
                }

                statics[thread_id].increment();
                R2_YIELD;
              }

              // exit
              R2_STOP(); // stop the scheduler for an elegant stop
              R2_RET;    // call the R2 keyword for return
            });
          }

          LOG(2) << "sanity check done for thread: " << thread_id;
          R2_RET;
        });

        r.run();

        ASSERT(max_key != 0);
        LOG(2) << "get worker [" << thread_id << "] with max_key: " << max_key;

        return 0;
      }));
    } // end creating main get() threads

    // spawn a polling thread for update sc
    handlers.push_back(new Worker(
      [&, my_id, server_addr, concurrency, server_port, server_host] {
        // the last thread is the model update thread
        usize thread_id = num_threads - 1;

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

        u64 remote_connect_id = 0;
        // u64 remote_connect_id = thread_id;
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

        r.spawnr([&](R2_ASYNC) {
          auto id = R2_COR_ID();

          auto ret = rpc.start_handshake(addr, R2_ASYNC_WAIT2);
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
                                              R2_ASYNC_WAIT2);
          ASSERT(qp != nullptr);
          LOG(4) << "create connected qp done";

          sc =
            ModelFetcher::bootstrap_remote_sc(0, rpc, addr, qp, R2_ASYNC_WAIT2);
          bar.wait();
          LOG(4) << "sc update thread exit";
          R2_RET;
        });

        r.run();
        return 0;
      })); // end create update SC thread

    ASSERT(handlers.size() == num_threads)
      << "this workload reserve two threads for other usage. So please use at "
         "least 3 threads.";
    return handlers;
  }
};

} // namespace bench

} // namespace fstore
