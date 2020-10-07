// compared with dynamic workload defined in dynamic.hpp, which uses one
// dedicated thread for insertions, dynamic_v2 uses a workload rand approach, so
// that 95% of requests are get while 5% workloads are insert

#include "r2/src/rdma/connect_manager.hpp"
#include "../../arc/val/net_config.hh"

namespace fstore {

namespace bench {

  const constexpr usize scan_ratio = 95;
  //const constexpr usize scan_ratio = 101;

class DynamicScan
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
    LOG(4) << "client use dynamic changing workloads, V2";
    std::vector<Worker*> handlers;

    for(uint i = 0;i < num_threads - 1;++i) {
      comm_channels.push_back(new r2::Channel<u32>(64));
    }

    {
      //          YCSBGenerator it(0, FLAGS_num, 0xdeadbeaf);
      YCSBHashGenereator it(0, FLAGS_num);

      // first we calculate the max key
      for (it.begin(); it.valid(); it.next())
        max_key = std::max(max_key, it.key());
    }

    // spawn worker thread for RPC
    for (usize thread_id = 0; thread_id < num_threads - 1; thread_id += 1) {
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

        auto nic_id = ::fstore::platforms::VALNic::choose_nic(thread_id);
        ASSERT(nic_id < all_devices.size()) << "wrong dev id:" << nic_id;
        RNic nic(all_devices[VALNic::choose_nic(thread_id)]);

        usize local_buf_sz = 1024 * 1024 * 16;
        char reply_buf[1024 * 1024];
        char* local_buf = new char[local_buf_sz];
        ASSERT(local_buf != nullptr);

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

        //u64 remote_connect_id = 0;
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

        LOG(0) << "Dynamic client #" << thread_id << " bootstrap done, "
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

#if 1
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
#else
          // now we use the connect manager for us to create QP
          rdma::SyncCM cm(::rdmaio::make_id(server_host, server_port),1000000.0);
          LOG(4) << "register with qp id: "<< qp_id;
          auto mr_res =
            cm.get_mr(thread_id); // this function will retry if failed
          ASSERT(std::get<0>(mr_res) == SUCC);

          RemoteMemory::Attr local_mr;
          ASSERT(global_rdma_ctrl().mr_factory.fetch_local_mr(thread_id, local_mr) == SUCC);
          auto qp = new RCQP(nic,std::get<1>(mr_res),local_mr,QPConfig());

          auto cm_ret =
            cm.cc_for_rc(qp,
                         {
                          .qp_id = qp_id, // the remote QP id for our connection
                          .nic_id = thread_id, // the remote NIC we want our QP to create at
                          .attr = qp->get_attr(), // remote QP needs to connect
                          .config = QPConfig(),   // we use the default connector
                         },
                         QPConfig());
          ASSERT(cm_ret == SUCC) << "error cm return: " << cm_ret;
#endif
          ASSERT(qp != nullptr);
          //LOG(4) << "create connected qp done";

          // pass
          bar.wait();
          auto server_meta =
            Helper::fetch_server_meta(addr, rpc, R2_ASYNC_WAIT2);

          // TODO: spawn real coroutine functions
          for (uint i = 0; i < concurrency; ++i) {
            r.spawnr([&, i, server_meta](R2_ASYNC) {
              RemoteMemory::Attr local_mr;
              ASSERT(global_rdma_ctrl().mr_factory.fetch_local_mr(
                       thread_id + 1024, local_mr) == SUCC);
              adapter->reset_key(local_mr.key);

              qp->local_mem_ = local_mr;

              FastRandom rand(0xdeadbeaf + 0xddd * R2_COR_ID() +
                              thread_id * 73);

              FastRandom rand1(0xdeadbeaf + 0xddd * R2_COR_ID() +
                               thread_id * 73 + 73);

#if 0
              auto local_buf =
                (char*)AllocatorMaster<>::get_thread_allocator()->alloc(409600);
              ASSERT(local_buf != nullptr);
#endif

              char* cor_send_buf = local_buf + 1024 * 1024 * i;

              ASSERT(max_key != 0) << "max key not initialized!";

              u64 counter = 0;
              u64 insert_count = 0;
              u64 scan_count = 0;

#if 0
              YCSBScanWorkload workload(FLAGS_max_key,
                                        thread_random_seed + R2_COR_ID() * 0xddd + 0xaaa,
                                        FLAGS_need_hash);
#endif

              utils::DistReport<usize> report;

              auto xc =
                new XCacheClient(xcache, &rpc, qp, server_meta.page_addr,server_meta.global_rdma_addr);


              const int N = 100000;
              ::fstore::FixedVector<N> inserted_keys;
#if 1
              for (uint i = 0; i < N; ++i) {
                u64 key = Hasher::hash(rand.next() % (FLAGS_num)); // uniform
                inserted_keys.emplace(key);
              }
              utils::ZipFanD zip_generator(
                N, thread_random_seed + R2_COR_ID() * 0xddd + 0xaaa);
#endif

              while (running) {
                auto ratio = rand.next() % 100;

                if (ratio < FLAGS_get_ratio
                    //|| (thread_id != 0) || my_id != 1
                    ) {

#if 0
                  u64 key = Hasher::hash(rand.next() %
                                         (FLAGS_num + insert_count)); // uniform
#else
                  auto zip_n = zip_generator.next();
                  auto key = inserted_keys.get_latest(zip_n);
#endif
                  int num = rand1.next() % 100 + 1;

                  SearchCode res = Ok;
                  if (!FLAGS_force_rpc) {
                    res = xc->scan(key,num,cor_send_buf,R2_ASYNC_WAIT);
                  } else {
                    res = Fallback;
                  }

                  switch (res) {
                    case SearchCode::Ok:
                      break;
                    case SearchCode::Invalid:
                      comm_channels[thread_id]->enqueue(xc->core->select_submodel(key)); // the model need update
                      //statics[thread_id].increment2();
                  case SearchCode::Unsafe: // XD: I've calarbrated that rarely
                    // it enters fallback
                    statics[thread_id].increment2();
                  case SearchCode::Fallback:
                    {
                    int expected_replies = std::ceil(
                      (num * ValType::get_payload()) / 4000.0);

                    statics[thread_id].increment3();

                    char* msg_buf = cor_send_buf + 64;
                    // fallback to RPC for execution
                    ScanPayload req = {
                      .table_id = 0, .start = key, .num = num
                    };
                    Marshal<ScanPayload>::serialize_to(req, msg_buf);
                    ASSERT(req.num > 0);

                    //char *scan_reply_buf = new char[expected_replies * 4000 + 64];

                    auto ret = rpc.call({ .cor_id = R2_COR_ID(), .dest = addr },
                                        SCAN_RPC,
                                        { .send_buf = msg_buf,
                                          .len = sizeof(ScanPayload),
                                          .reply_buf = cor_send_buf,
                                          .reply_cnt = expected_replies });

                    ASSERT(ret == SUCC);
                    R2_PAUSE;
                  } break;
                    // delete scan_reply_buf;
                  default:
                    ASSERT(false) << " error ret: " << res;
                  }

                  //ASSERT(false) << 233;
                  scan_count += 1;
                } // end processing get
                else {
                  insert_count += 1;
                  u64 key = rand.next() % (max_key + insert_count);
                  char* msg_buf = cor_send_buf + 64;
                  char reply_buf[128];

                  // process insert
                  using PutPayload =
                    GetPayload; // put uses the same header's argument as get
                  PutPayload req = { .table_id = 0, .key = key };

                  Marshal<PutPayload>::serialize_to(req, msg_buf);

                  //
                  auto ret =
                    rpc.call({ .cor_id = R2_COR_ID(), .dest = addr },
                             INSERT_ID,
                             { .send_buf = msg_buf,
                               .len = sizeof(PutPayload) + sizeof(ValType),
                               .reply_buf = reply_buf,
                               .reply_cnt = 1 });
                  ASSERT(ret == SUCC) << "get err ret call: " << ret;

                  //              R2_PAUSE_AND_YIELD;
                  R2_EXECUTOR.wait_for(50000000000L);
                  auto res = R2_PAUSE; // pause for the result
                  ASSERT(res == SUCC)
                    << "got a wrong poll result for RPC : " << res;

                  inserted_keys.emplace(key);

                  // statics[thread_id].increment1(); // add to another counter
                }

                statics[thread_id].increment();
                counter += 1;
                R2_YIELD;
              }

              // exit
              R2_STOP(); // stop the scheduler for an elegant stop
              R2_RET;    // call the R2 keyword for return
            });
          }

          //LOG(2) << "sanity check done for thread: " << thread_id;
          R2_RET;
        });

        r.run();

        ASSERT(max_key != 0);
        //LOG(2) << "get worker [" << thread_id << "] with max_key: " << max_key;

        return 0;
      }));
    } // end creating main get() threads

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
#if 1
        auto qp = Helper::create_connect_qp(global_rdma_ctrl().mr_factory,
                                            global_rdma_ctrl().qp_factory,
                                            nic,
                                            qp_id,
                                            thread_id,
                                            remote_mr,
                                            addr,
                                            rpc,
                                            R2_ASYNC_WAIT2);
#else
        // now we use the connect manager for us to create QP
        rdma::SyncCM cm(::rdmaio::make_id(server_host, server_port));
        LOG(4) << "register with qp id: " << qp_id;
        auto mr_res =
          cm.get_mr(thread_id); // this function will retry if failed
        ASSERT(std::get<0>(mr_res) == SUCC);

        RemoteMemory::Attr local_mr;
        ASSERT(global_rdma_ctrl().mr_factory.fetch_local_mr(thread_id,
                                                            local_mr) == SUCC);
        auto qp = new RCQP(nic, std::get<1>(mr_res), local_mr, QPConfig());

        auto cm_ret = cm.cc_for_rc(
          qp,
          {
            .qp_id = qp_id,      // the remote QP id for our connection
            .nic_id = thread_id, // the remote NIC we want our QP to create at
            .attr = qp->get_attr(), // remote QP needs to connect
            .config = QPConfig(),   // we use the default connector
          },
          QPConfig());
        ASSERT(cm_ret == SUCC) << "error cm return: " << cm_ret;
        {
          char* local_buffer =
            (char*)AllocatorMaster<>::get_thread_allocator()->alloc(
              sizeof(u64));
          rdma::SROp op(qp);
          op.set_payload(local_buffer,
                         sizeof(u64)); // we want read a u64 to the local buffer
          op.set_read().set_remote_addr(
            0);                         // this is a read, and at remote addr 0
          auto ret = op.execute_sync(); // execute the request in a sync,
                                        // blocking fashion.
          ASSERT(std::get<0>(ret) == SUCC); // check excecute results
          AllocatorMaster<>::get_thread_allocator()->dealloc(local_buffer);
        }
#endif
        ASSERT(qp != nullptr);

        LOG(4) << "start to bootstrap xcache!!!!!!!!!!!!!!!!!!";
#if 1
        // initialize the XCache boot
        auto retb = XBoot::bootstrap_xcache(0, rpc, addr, qp, R2_ASYNC_WAIT);
        auto xboot = std::get<0>(retb);
        xcache = std::get<1>(retb);
        // ASSERT(false);

        r2::Timer t;
#if 0 // per-model update
        char* send_buf =
          (char*)AllocatorMaster<>::get_thread_allocator()->alloc(4096);

        for (uint i = 0; i < xcache->submodels.size(); ++i) {
          xcache->submodels[i] =
            xboot.update_submodel(i, send_buf, R2_ASYNC_WAIT);
        }

        AllocatorMaster<>::get_thread_allocator()->dealloc(send_buf);
#else
        // batch update all
        // char* send_buf =
        //                (char*)AllocatorMaster<>::get_thread_allocator()->alloc(1024
        //                * xcache->submodels.size());
        char* send_buf =
          (char*)AllocatorMaster<>::get_thread_allocator()->alloc(
            xboot.max_read_length);

        ASSERT(send_buf != nullptr);
        xboot.batch_update_all(xcache->submodels, true, send_buf, R2_ASYNC_WAIT);

        //AllocatorMaster<>::get_thread_allocator()->dealloc(send_buf);

#endif

#endif

        LOG(4) << "update xcache done using " << t.passed_msec() / 1000.0
               << " ms";
        //ASSERT(false);
        bar.wait();

        //
#if 0
        r2::Timer all_t;
        while(running && true) {
          r2::compile_fence();

          // update the xcache
          t.reset();
#if 0
          xboot.batch_update_all(xcache->submodels, false, send_buf, R2_ASYNC_WAIT);
#else
          for (uint i = 0; i < xcache->submodels.size(); ++i) {
            //xcache->submodels[i] =
            //              xboot.update_submodel(i, send_buf, R2_ASYNC_WAIT);
            xboot.update_submodel(xcache->submodels[i], i, send_buf, R2_ASYNC_WAIT);
          }
#endif
          LOG(4) << "retrain update batch using: " << t.passed_msec() / 1000.0 << " ms";
          sleep(FLAGS_model_update_gap);
          //break;
          if (all_t.passed_sec() > 70) {
            LOG(4) << "model update thread exit";
            break;
          }
        }
#endif
        for(uint i = 0; i < num_threads - 1;++i) {
          R2_EXECUTOR.spawnr([&, i](R2_ASYNC) {
                               auto c = comm_channels[i];
                               char *my_buf = send_buf + i * 40960;

                               while (true) {
                                 auto ret = c->dequeue();
                                 if (ret) {
                                   auto res =
                                     xboot.update_submodel(xcache->submodels[ret.value()],
                                                           ret.value(),
                                                           my_buf,
                                                           R2_ASYNC_WAIT);
                                   if (unlikely(!res)) {
                                     // retry
                                     xboot.update_submodel(
                                       xcache->submodels[ret.value()],
                                       ret.value(),
                                       my_buf,
                                       R2_ASYNC_WAIT);
                                   }
                                 }
                                 R2_YIELD;
                               }
          });
        }
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
