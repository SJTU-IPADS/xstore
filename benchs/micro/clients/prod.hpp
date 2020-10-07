
#pragma once

#include "../../../cli/libx.hpp"

#include "../../../src/data_sources/nutanix/workloads.hpp"

#include "../../../xcli/mod.hh"

namespace fstore {

namespace bench {

  //std::shared_ptr<XDirectTopLayer> xcache = nullptr;

DEFINE_int64(write_ratio, 58, "the ratio to write");

class Product
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
    LOG(4) << "client use production workload";
    std::vector<u64>* all_keys = new std::vector<u64>();
#if 0
    std::fstream fs("keys.txt", std::ofstream::in);
    u64 temp;
    while (fs >> temp) {
      all_keys->push_back(temp);
    }
#endif
    // we don't free all_keys as it will live long enough
    std::vector<Worker*> handlers;

    for (usize thread_id = 0; thread_id < num_threads - 1; thread_id += 1) {
      // pass
      // spawn a polling thread for update sc
      handlers.push_back(new Worker([&,
                                     all_keys,
                                     thread_id,
                                     my_id,
                                     server_addr,
                                     concurrency,
                                     server_port,
                                     server_host] {
        // first we build RDMA connections
        Addr addr({ .mac_id = server_addr.mac_id, .thread_id = thread_id });
        Addr insert_addr({ .mac_id = server_addr.mac_id, .thread_id = 1 });

        auto all_devices = RNicInfo::query_dev_names();
        ASSERT(!all_devices.empty()) << "RDMA must be supported.";

        auto nic_id = ::fstore::platforms::VALNic::choose_nic(thread_id);
        ASSERT(nic_id < all_devices.size()) << "wrong dev id:" << nic_id;
        RNic nic(all_devices[VALNic::choose_nic(thread_id)]);

        usize local_buf_sz = 4096 * 4096 * 128u;
        char* local_buf = new char[local_buf_sz];
        assert(local_buf != nullptr);

        // First we register the memory
        auto ret = global_rdma_ctrl().mr_factory.register_mr(
          thread_id,
          global_memory_region().base_mem_,
          global_memory_region().base_size_,
          nic);
        ASSERT(ret == SUCC)
          << "failed to register memory region for thread: " << thread_id;

        ret = global_rdma_ctrl().mr_factory.register_mr(
          thread_id + 1024,
          local_buf,
          local_buf_sz,
          // global_memory_region().base_mem_,
          // global_memory_region().base_size_,
          nic);
        ASSERT(ret == SUCC) << "failed to register memory region " << ret
                            << " with id: " << thread_id + 73;
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

        // also we use the first thread for all insertions
        while (adapter->connect(insert_addr,
                                ::rdmaio::make_id(server_host, server_port),
                                1) != SUCC) {
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

        u64 thread_random_seed = FLAGS_seed + 73 * thread_id;

        RScheduler r;
        RPC rpc(adapter);
        rpc.spawn_recv(r);

        //

        u64 qp_id = my_id << 32 | (thread_id + 1);

        r.spawnr([&](R2_ASYNC) {
          auto id = R2_COR_ID();

          auto ret = rpc.start_handshake(addr, R2_ASYNC_WAIT2);
          ret = rpc.start_handshake(insert_addr, R2_ASYNC_WAIT2);
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
          if (thread_id == 0) {
            // sc = ModelFetcher::bootstrap_remote_sc(
            // 0, rpc, server_addr, qp, r, h);

            auto ret =
              XBoot::bootstrap_xcache(0, rpc, addr, qp, R2_ASYNC_WAIT);
            auto xboot = std::get<0>(ret);
            xcache = std::get<1>(ret);

            char* send_buf =
              (char*)AllocatorMaster<>::get_thread_allocator()->alloc(40960);

            for (uint i = 0; i < xcache->submodels.size(); ++i) {
              //LOG(4) << "update model: " << i; sleep(1);
              // xcache->submodels[i] = xboot.update_submodel(i,send_buf,h,r);
              xboot.update_submodel(xcache->submodels[i], i, send_buf, R2_ASYNC_WAIT);
            }

            AllocatorMaster<>::get_thread_allocator()->dealloc(send_buf);
          }
          bar.wait();

          auto server_meta =
            Helper::fetch_server_meta(addr, rpc, R2_ASYNC_WAIT2);

          auto xc = new XCacheClient(xcache,
                                     &rpc,
                                     qp,
                                     server_meta.page_addr,
                                     server_meta.global_rdma_addr);

          // TODO: spawn real coroutine functions
          for (uint i = 0; i < concurrency; ++i) {
            r.spawnr([&, i, server_meta](R2_ASYNC) {
              RemoteMemory::Attr local_mr;
              ASSERT(global_rdma_ctrl().mr_factory.fetch_local_mr(
                       thread_id + 1024, local_mr) == SUCC);
              adapter->reset_key(local_mr.key);

              FastRandom rand(0xdeadbeaf + 0xddd * R2_COR_ID() +
                              thread_id * 73);

              FastRandom rand1(0xdeadbeaf + 0xddd * R2_COR_ID() +
                               thread_id * 73 + 1);

              YCSBCWorkload ycsb(FLAGS_num,
                                 thread_random_seed + R2_COR_ID() * 0xddd +
                                   0xaaa,
                                 FLAGS_need_hash);

#if 0
              auto local_buf =
                (char*)AllocatorMaster<>::get_thread_allocator()->alloc(40960);
              ASSERT(local_buf != nullptr);
#endif
              ::fstore::sources::Nut0Workload workload(
                thread_random_seed + R2_COR_ID() * 0xddd + 0xaaa);

              char* send_buf = local_buf + 4096 * 4096 * 2 * i;

              u64 counter = 0;
              u64 insert_count = 0;
              utils::DistReport<usize> report;

              /* statics:
                 data0 : thpt
                 data1 : get thpt
                 data1 : fallback thpt
                 data2 : invalid thpt
               */
              qp->local_mem_ = local_mr;
              while (running) {
                auto fetched_sc = (SC*)ssc;
                r2::compile_fence();

                // XClient x(addr, &fc, &rpc);

                // roll a key
#if 1
                auto key = workload.next_key();
                // auto key = rand.next() % 500000000;
#else
                auto key_idx = rand.next() % all_keys->size();
                auto key = (*all_keys)[key_idx];
#endif

#if 0
                if (R2_COR_ID() == 2 && thread_id == 4) {
                  auto predict = fc.get_predict(key);
                  auto span = fc.get_page_span(predict);
                  report.add(span.second - span.first + 1);

                  if (counter % 100000 == 0) {
                    LOG(3) << "exam results, min: " << report.min
                           << " ; max: " << report.max
                           << " avg: " << report.average;
                  }
                  counter += 1;
                }
#endif

                // then we get the workload
                // auto workload_percet = rand1.next() % 100;
                auto workload_percet = rand.next_uniform();
                if (workload_percet <
                    static_cast<double>(FLAGS_write_ratio) / 100) {
                  /// put case
                  ValType val;
                  // ASSERT(x.put(key,val, send_buf,R2_ASYNC_WAIT) == SUCC);

                  // x.get_rpc(key,send_buf,R2_ASYNC_WAIT);

                  char reply_buf[sizeof(ValType)];

                  auto msg_buf = send_buf + rpc.reserved_header_sz();

                  using namespace ::fstore::server;
                  GetPayload req = { .table_id = 0, .key = key };
                  Marshal<GetPayload>::serialize_to(req, msg_buf);

                  auto ret =
                    rpc.call({ .cor_id = R2_COR_ID(), .dest = addr },
                             PUT_ID,
                             { .send_buf = msg_buf,
                               .len = sizeof(GetPayload) + sizeof(ValType),
                               .reply_buf = reply_buf,
                               .reply_cnt = 1 });

                  R2_PAUSE;

                  statics[thread_id].increment1();
                } else if (workload_percet < 0.98) {
                  /// get case

                  //FClient::GetResult res;
                  SearchCode res;
                  if (FLAGS_force_rpc) {
                    // res = x.get_rpc(key, send_buf,R2_ASYNC_WAIT);
                    char reply_buf[sizeof(ValType)];

                    auto& factory = rpc.get_buf_factory();
                    // auto send_buf = factory.get_inline_buf();
                    // auto send_buf = factory.alloc(sizeof(ValType) +
                    // sizeof(GetPayload));
                    auto msg_buf = send_buf + rpc.reserved_header_sz();

                    using namespace ::fstore::server;
                    GetPayload req = { .table_id = 0, .key = key };
                    Marshal<GetPayload>::serialize_to(req, msg_buf);

                    auto ret =
                      rpc.call({ .cor_id = R2_COR_ID(), .dest = addr },
                               GET_ID,
                               { .send_buf = msg_buf,
                                 .len = sizeof(GetPayload),
                                 .reply_buf = reply_buf,
                                 .reply_cnt = 1 });

                    R2_PAUSE;

                  } else {
                    res = xc->get_direct(key, send_buf, R2_ASYNC_WAIT);
                  }

                  // record execution status
                  switch (res) {
                    case SearchCode::Unsafe:
                      // recorded as invalid
                      statics[thread_id].increment2();
                      break;
                    case SearchCode::None:
                      ASSERT(false);
                      // recorded as fallback
                      statics[thread_id].increment3();
                      break;
                    default:
                      break;
                  }
                } else {
                  /// scan case
                  usize scan_num = rand1.next() % 99 + 1;
                  //
                  if (FLAGS_force_rpc) {
                    int expected_replies =
                      std::ceil((scan_num * ValType::get_payload()) / 4000.0);


                    char* msg_buf = send_buf + 64;
                    // fallback to RPC for execution
                    ScanPayload req = { .table_id = 0,
                                        .start = key,
                                        .num = scan_num };
                    Marshal<ScanPayload>::serialize_to(req, msg_buf);
                    ASSERT(req.num > 0);


                    auto ret = rpc.call({ .cor_id = R2_COR_ID(), .dest = addr },
                                        SCAN_RPC,
                                        { .send_buf = msg_buf,
                                          .len = sizeof(ScanPayload),
                                          .reply_buf = msg_buf,
                                          .reply_cnt = expected_replies });

                    ASSERT(ret == SUCC);
                    R2_PAUSE;

                  } else {
                    auto res = xc->scan(key, scan_num, send_buf, R2_ASYNC_WAIT);
                    ASSERT(res == SearchCode::Ok);
                  }
                  statics[thread_id].increment3();
                }
                statics[thread_id].increment();
                R2_YIELD;
              }
              R2_STOP();
              R2_RET;
            });
          }
          R2_RET;
        });
        r.run();
        return 0;
      }));
    }

    // spawn a polling thread for update ssc
    handlers.push_back(new Worker([&,
                                   my_id,
                                   server_addr,
                                   concurrency,
                                   server_port,
                                   server_host,
                                   num_threads] {
      // the last thread is the model update thread
      usize thread_id = num_threads - 1;

      Addr addr({ .mac_id = server_addr.mac_id, .thread_id = thread_id });

      auto all_devices = RNicInfo::query_dev_names();
      ASSERT(!all_devices.empty()) << "RDMA must be supported.";

      auto nic_id = ::fstore::platforms::VALNic::choose_nic(thread_id);
      // usize nic_id = 1;
      // LOG(4) << "choose nic " << nic_id << " for thread: " << thread_id;

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
      // LOG(3) << "SC thread connects to : " << remote_connect_id;
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

        //ssc = (volatile SC*)ModelFetcher::bootstrap_remote_sc(
        //          0, rpc, addr, qp, R2_ASYNC_WAIT2);
        bar.wait();

        Timer t;
        usize count = 0;
        u64 sum = 0;

        // LOG(4) << "SC update thread, running: " << running << " " << count;

        std::map<int, SC*> gc;
        int epoch = 1;
        double update_gap = 1000000.0;

        while (running && false) {
          r2::compile_fence();
          // update the sc periodically

          if (t.passed_msec() < update_gap) {
            // usleep(100);
            continue;
          }

          gc.insert(std::make_pair(epoch, (SC*)ssc));
          epoch += 1;
          ssc = (volatile SC*)ModelFetcher::bootstrap_remote_sc(
            0, rpc, addr, qp, R2_ASYNC_WAIT2);
          // LOG(4) << "update one sc done: " << count;
          t.reset();

          sum += ((SC*)ssc)->get_predict(count).pos;

          r2::compile_fence();
          count += 1;

          int pre_epoch = 5;
          if (epoch >= pre_epoch) {
            if (gc.find(epoch - pre_epoch) != gc.end()) {
              auto it = gc.find(epoch - pre_epoch);
              delete it->second;
              gc.erase(it);
            }
          }

          sum += (u64)ssc;
        }
        LOG(4) << "sc update thread exit: " << sum;
        R2_STOP();
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

}

}
