#pragma once

#include "../../../xcli/cached_mod_v3.hh"

namespace fstore {

namespace bench {

std::vector<CBM*>* global_model_cache_1 = nullptr;

class DE
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
    LOG(4) << "client use dynamic changing workloads, V4";
    std::vector<Worker*> handlers;

    ASSERT(updater_thread_num >= 1);
    for (uint i = 0; i < num_threads; ++i) {
      comm_channels.push_back(new r2::Channel<u32>(128));
      comm_channels_1.push_back(new r2::Channel<u32>(128));
    }

    {
      //          YCSBGenerator it(0, FLAGS_num, 0xdeadbeaf);
      YCSBHashGenereator it(0, FLAGS_num);

      // first we calculate the max key
      for (it.begin(); it.valid(); it.next())
        max_key = std::max(max_key, it.key());
    }

    // spawn worker thread for RPC
    for (usize thread_id = 0; thread_id < num_threads; thread_id += 1) {
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
        Addr insert_addr({ .mac_id = server_addr.mac_id, .thread_id = 1 });

        auto all_devices = RNicInfo::query_dev_names();
        ASSERT(!all_devices.empty()) << "RDMA must be supported.";

        auto nic_id = ::fstore::platforms::VALNic::choose_nic(thread_id);
        ASSERT(nic_id < all_devices.size()) << "wrong dev id:" << nic_id;
        RNic nic(all_devices[VALNic::choose_nic(thread_id)]);

        usize local_buf_sz = 4096 * 4096 * 10;
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
        /**
         * Fetch server's MR
         */
        RemoteMemory::Attr remote_mr;
        while (RMemoryFactory::fetch_remote_mr(
                 remote_connect_id,
                 ::rdmaio::make_id(server_host, server_port),
                 remote_mr) != SUCC) {
        }

        // LOG(4) << "Dynamic client #" << thread_id << " bootstrap done, "
        //<< " will create: [" << concurrency
        //<< "] coroutines for execution.";

        u64 thread_random_seed = FLAGS_seed + 73 * thread_id;

        RScheduler r;
        RPC rpc(adapter);
        rpc.spawn_recv(r);

        u64 qp_id = my_id << 32 | (thread_id + 1);

        r.spawnr([&](R2_ASYNC) {
          auto id = R2_COR_ID();

          auto ret = rpc.start_handshake(addr, R2_ASYNC_WAIT2);
          // ret = rpc.start_handshake(insert_addr, R2_ASYNC_WAIT2);
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
          rdma::SyncCM cm(::rdmaio::make_id(server_host, server_port));
          LOG(4) << "register with qp id: " << qp_id;
          auto mr_res =
            cm.get_mr(thread_id); // this function will retry if failed
          ASSERT(std::get<0>(mr_res) == SUCC);

          RemoteMemory::Attr local_mr;
          ASSERT(global_rdma_ctrl().mr_factory.fetch_local_mr(
                   thread_id, local_mr) == SUCC);
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
#endif
          ASSERT(qp != nullptr);
          // LOG(4) << "create connected qp done";

          auto server_meta =
            Helper::fetch_server_meta(addr, rpc, R2_ASYNC_WAIT2);

          auto page_start = server_meta.page_addr;
          auto x = XCachedClientV3::create(
            0, 10000000, rpc, addr, page_start, R2_ASYNC_WAIT);
          XCachedClientV3 *x2 = nullptr;
          {
            RemoteMemory::Attr local_mr;
            ASSERT(global_rdma_ctrl().mr_factory.fetch_local_mr(
                     thread_id + 1024, local_mr) == SUCC);
            qp->local_mem_ = local_mr;
          }
          x->qp = qp;
          if (thread_id == 0) {
            global_model_cache =
              new std::vector<CBM*>(x->num_total_model, nullptr);
            x->model_cache = global_model_cache;
            x->fill_all_submodels(local_buf, R2_ASYNC_WAIT);
          }

          // TODO: spawn real coroutine functions
          for (uint i = 0; i < concurrency; ++i) {
            r.spawnr([&, i, server_meta](R2_ASYNC) {
              RemoteMemory::Attr local_mr;
              ASSERT(global_rdma_ctrl().mr_factory.fetch_local_mr(
                       thread_id + 1024, local_mr) == SUCC);
              adapter->reset_key(local_mr.key);

              FastRandom rand(0xdeadbeaf * FLAGS_id + 0xddd * R2_COR_ID() +
                              thread_id * 73);

              FastRandom rand1(0xdeadbeaf * FLAGS_id + 0xddd * R2_COR_ID() +
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

              char* send_buf = local_buf + 4096 * 1024 * i;
              ASSERT(max_key != 0) << "max key not initialized!";

              u64 counter = 0;
              u64 insert_count = 0;

              /* statics:
                 data0 : thpt
                 data1 : get thpt
                 data1 : fallback thpt
                 data2 : invalid thpt
               */
              qp->local_mem_ = local_mr;

              r2::Timer t;
              utils::DistReport<usize> report;

              char reply_buf[4096];

              while (running) {

                if (t.passed_sec() > 20) {
                  break;
                }

                if (((thread_id == 0) || (thread_id == 1)) && FLAGS_id == 1 && true && i == 0) {
                  if (t.passed_sec() < 10) {
                    R2_YIELD;
                    continue;
                  }
                  // insert thread
                  u64 key = rand1.next() % (max_key);
                  char* msg_buf = send_buf + 64;

                  // process insert
                  using PutPayload = GetPayload; // put uses the same header's
                                                 // argument as get
                  PutPayload req = { .table_id = 0, .key = key };

                  Marshal<PutPayload>::serialize_to(req, msg_buf);

                  //
                  auto ret =
                    rpc.call({ .cor_id = R2_COR_ID(), .dest = addr },
                             // rpc.call({ .cor_id = R2_COR_ID(), .dest =
                             // insert_addr },
                             INSERT_ID,
                             // GET_ID,
                             { .send_buf = msg_buf,
                               .len = sizeof(PutPayload) + sizeof(ValType),
                               .reply_buf = reply_buf,
                               .reply_cnt = 1 });
                  ASSERT(ret == SUCC) << "get err ret call: " << ret;

                  //              R2_PAUSE_AND_YIELD;
                  // R2_EXECUTOR.wait_for(5000000000L);
                  auto res = R2_PAUSE; // pause for the result
                  ASSERT(res == SUCC)
                    << "got a wrong poll result for RPC : " << res;
                  R2_YIELD;
                }

                u64 key = 0;
                key = Hasher::hash(rand.next() % (FLAGS_num)); // uniform

                // default XStore get
                // LOG(4) << "XStore get key: " << key;
                SearchCode res = Ok;
                if (!FLAGS_force_rpc) {
                  // res = xc->get_direct(key, send_buf, R2_ASYNC_WAIT);
                  // res = xc->get_direct_possible_non_exist(key, send_buf,
                  // R2_ASYNC_WAIT);
                  err_num = 0;
                  res = x->get_direct(key, send_buf, R2_ASYNC_WAIT);
                  report.add(err_num);

                } else {
                  // force to use RPC
                  res = Fallback;
                }
                switch (res) {
                  case SearchCode::Ok:
                    break;
                  case SearchCode::Unsafe: // XD: I've calarbrated that rarely
                                           // it enters fallback
                  {
                    GetPayload req = { .table_id = 0, .key = key };
                    char* msg_buf =
                      send_buf + 64; // add a padding to store rpc header

                    Marshal<GetPayload>::serialize_to(req, msg_buf);

                    auto ret = rpc.call({ .cor_id = R2_COR_ID(), .dest = addr },
                                        GET_ID,
                                        { .send_buf = msg_buf,
                                          .len = sizeof(GetPayload),
                                          .reply_buf = reply_buf,
                                          .reply_cnt = 1 });

                    // R2_EXECUTOR.wait_for(10000000000L);
                    auto res = R2_PAUSE;
                    ASSERT(res == SUCC);

                  } break;
                  case SearchCode::Invalid:
                    // statics[thread_id].increment2();
                    statics[thread_id].increment2();

                  case SearchCode::Fallback: {
                    // auto model = xc->core->select_submodel(key);
                    statics[thread_id].increment3();
#if 1
                    {


                      auto model_id = x->select_submodel(key);
                      GetPayload req = {
                        .table_id = 0,
                        .key = key,
                        .model_id = model_id,
                        .model_seq =
                          (*x->model_cache)[model_id]->train_watermark,
                        .model_key = x->submodels_addr
                      };
                      char* msg_buf =
                        send_buf + 64; // add a padding to store rpc header

                      Marshal<GetPayload>::serialize_to(req, msg_buf);
                      // LOG(4) << "need to update:" << model_id;
                      // sleep(1);
                      //char reply_buf[1024];
                      auto ret =
                        rpc.call({ .cor_id = R2_COR_ID(), .dest = addr },
                                 //GET_ID,
                                 PREDICT, // in this context, predict means
                                 // get_w_fallback
                                 { .send_buf = msg_buf,
                                   .len = sizeof(GetPayload),
                                   .reply_buf = reply_buf,
                                   .reply_cnt = 1 });

                      auto res = R2_PAUSE;
                      ASSERT(res == SUCC)
                        << "r2 wait res: " << res << " in get() fallback path";

                      u32 cur_model_sz = *((u32*)reply_buf);
                      if (cur_model_sz != 0) {
                        Option<u64> ext_addr = {};

                        auto& model = *((*x->model_cache)[model_id]);
                        model.lock.lock();
                        auto cached_seq = model.seq;
                        model.seq = 0; // invalid

                        r2::compile_fence();
                        auto ret = Serializer::direct_extract(
                          reply_buf + sizeof(u32), model);
                        ASSERT(ret);

                        if (ext_addr) {
                          ASSERT(false) << " current not supported ";
                        }
                        r2::compile_fence();
                        model.seq = cached_seq += 1;
                        r2::compile_fence();
                        model.lock.unlock();
                      } else {
                        // not update
                        //ASSERT(false);
                      }
                    }
#endif
                  } break;
                  case SearchCode::None:
                    ASSERT(false);
#if 0
                      LOG(4) << "zip_n: " << zip_n << " " << key;

                      xc->debug_get(key,send_buf, R2_ASYNC_WAIT);
                      ASSERT(false);
#endif
                    break;
                }
                statics[thread_id].increment();
                counter += 1;

                if (FLAGS_id == 1 && thread_id == 0 && i == 0) {
                  if (counter >= 500000) {
                    LOG(3) << "exam results, min: " << report.min
                           << " ; max: " << report.max
                           << " avg: " << report.average;
                    report.clear();
                    counter = 0;
                  }
                }
                R2_YIELD;
              }
#if 0
              t.reset();
              while (running) {

                u64 key = rand1.next() % (max_key);
                char* msg_buf = send_buf + 64;
                //char reply_buf[128];

                // process insert
                using PutPayload = GetPayload; // put uses the same header's
                                               // argument as get
                PutPayload req = { .table_id = 0, .key = key };

                Marshal<PutPayload>::serialize_to(req, msg_buf);

                //
                auto ret =
                  rpc.call({ .cor_id = R2_COR_ID(), .dest = addr },
                           // rpc.call({ .cor_id = R2_COR_ID(), .dest =
                           // insert_addr },
                           INSERT_ID,
                           // GET_ID,
                           { .send_buf = msg_buf,
                             .len = sizeof(PutPayload) + sizeof(ValType),
                             .reply_buf = reply_buf,
                             .reply_cnt = 1 });
                ASSERT(ret == SUCC) << "get err ret call: " << ret;

                //              R2_PAUSE_AND_YIELD;
                // R2_EXECUTOR.wait_for(5000000000L);
                auto res = R2_PAUSE; // pause for the result
                ASSERT(res == SUCC)
                  << "got a wrong poll result for RPC : " << res;

                statics[thread_id].increment();
                counter += 1;
                R2_YIELD;
              }
#endif
              t.reset();

              bool notified = false;
              while (running) {
                r2::compile_fence();
                if (thread_id == 1 && (t.passed_sec() > 1) && i == 0) {
                    char* msg_buf = send_buf + 64;
                    // char reply_buf[128];
                    auto ret = rpc.call({ .cor_id = R2_COR_ID(), .dest = addr },
                                        NULL_ID,
                                        { .send_buf = msg_buf,
                                          .len = 0,
                                          .reply_buf = reply_buf,
                                          .reply_cnt = 1 });
                    ASSERT(ret == SUCC) << "get err ret call: " << ret;

                    auto res = R2_PAUSE; // pause for the result
                    ASSERT(res == SUCC)
                      << "got a wrong poll result for RPC : " << res;
                    // ASSERT(false);
                    // re-fill
                    x2 = XCachedClientV3::create(
                      0, 10000000, rpc, addr, page_start, R2_ASYNC_WAIT);
                    x2->qp = qp;
                    auto temp =
                      new std::vector<CBM*>(x2->num_total_model, nullptr);
                    x2->model_cache = temp;

                    if (FLAGS_id == 1) {
                      LOG(4)
                        << "init model with: " << x2->num_total_model << " s"
                        << "; max addr: " << x2->max_addr;
                      // ASSERT(false);
                    }
                    // ASSERT(false);
                    // x2->fill_all_submodels(local_buf, R2_ASYNC_WAIT);
                    // if (FLAGS_id == 1) {
                    if (0) {
                      // ASSERT(false);
                    }
                    r2::compile_fence();
                    temp->resize(x2->num_total_model);
                    for (uint j = 0; j < x2->num_total_model; ++j) {
                      (*temp)[j] = nullptr;
                    }
                    global_model_cache_1 = temp;
                    // ASSERT(false);
                    break;
                  }

                if (thread_id == 1 && (i != 0)) {
                  //ASSERT(false) << thread_id << " " << i;
                  R2_YIELD;
                  continue;
                }


                //if (((thread_id == 0) || (thread_id == 1)) && FLAGS_id == 1 && true) {
                if (((thread_id == 0) || (thread_id == 1)) && FLAGS_id == 1 &&
                    true && i == 0) {

                  // insert thread
                  u64 key = rand1.next() % (max_key);
                  char* msg_buf = send_buf + 64;

                  // process insert
                  using PutPayload = GetPayload; // put uses the same header's
                                                 // argument as get
                  PutPayload req = { .table_id = 0, .key = key };

                  Marshal<PutPayload>::serialize_to(req, msg_buf);

                  //
                  auto ret =
                    rpc.call({ .cor_id = R2_COR_ID(), .dest = addr },
                             // rpc.call({ .cor_id = R2_COR_ID(), .dest =
                             // insert_addr },
                             INSERT_ID,
                             // GET_ID,
                             { .send_buf = msg_buf,
                               .len = sizeof(PutPayload) + sizeof(ValType),
                               .reply_buf = reply_buf,
                               .reply_cnt = 1 });
                  ASSERT(ret == SUCC) << "get err ret call: " << ret;

                  //              R2_PAUSE_AND_YIELD;
                  // R2_EXECUTOR.wait_for(5000000000L);
                  auto res = R2_PAUSE; // pause for the result
                  ASSERT(res == SUCC)
                    << "got a wrong poll result for RPC : " << res;
                  R2_YIELD;
                }

                r2::compile_fence();
                if (global_model_cache_1 != nullptr && x2 == nullptr && i == 0) {
                  // update
                  x2 = XCachedClientV3::create(
                    0, 10000000, rpc, addr, page_start, R2_ASYNC_WAIT);
                  x2->qp = qp;
                  x2->model_cache = global_model_cache_1;
                  LOG(4) << "init x2 done!";
                }

                SearchCode res = Ok;
                u64 key = 0;
                key = Hasher::hash(rand.next() % (FLAGS_num)); // uniform

                // issue get
                err_num = 0;

                if (x2 == nullptr) {
                  // normal get
                  res = x->get_direct(key, send_buf, R2_ASYNC_WAIT);
                } else {
                  // use x2 get
                  res = x2->get_direct(key, send_buf, R2_ASYNC_WAIT);
                }
                report.add(err_num);
                //                res = Ok;
                switch (res) {
                  case SearchCode::Ok:
                    break;
                  case SearchCode::Unsafe: // XD: I've calarbrated that rarely
                                           // it enters fallback
                  {
                    GetPayload req = { .table_id = 0, .key = key };
                    char* msg_buf =
                      send_buf + 64; // add a padding to store rpc header

                    Marshal<GetPayload>::serialize_to(req, msg_buf);

                    //char reply_buf[512];
                    auto ret = rpc.call({ .cor_id = R2_COR_ID(), .dest = addr },
                                        GET_ID,
                                        { .send_buf = msg_buf,
                                          .len = sizeof(GetPayload),
                                          .reply_buf = reply_buf,
                                          .reply_cnt = 1 });

                    // R2_EXECUTOR.wait_for(10000000000L);
                    auto res = R2_PAUSE;
                    ASSERT(res == SUCC);

                  } break;
                  case SearchCode::Invalid:
                    // statics[thread_id].increment2();
                    statics[thread_id].increment2();

                  case SearchCode::Fallback: {
                    //break;
                    // auto model = xc->core->select_submodel(key);
                    statics[thread_id].increment3();
                    XCachedClientV3 *xx = nullptr;
#if 1
                    if (x2 != nullptr) {
                      xx = x2;
                    } else {
                      xx = x;
                    }
#endif
#if 1
                    {
                      auto model_id = xx->select_submodel(key);
                      ASSERT(model_id < xx->model_cache->size()) << " model id: "<< model_id;

                      if ((*xx->model_cache)[model_id] == nullptr) {
                        GetPayload req = {
                          .table_id = 0,
                          .key = key,

                        };
                        char* msg_buf =
                          send_buf + 64; // add a padding to store rpc header

                        Marshal<GetPayload>::serialize_to(req, msg_buf);
                        // LOG(4) << "need to update:" << model_id;
                        // sleep(1);

                        auto ret =
                          rpc.call({ .cor_id = R2_COR_ID(), .dest = addr },
                                   GET_ID,
                                   //PREDICT, // in this context, predict means
                                   // get_w_fallback
                                   { .send_buf = msg_buf,
                                     .len = sizeof(GetPayload),
                                     .reply_buf = reply_buf,
                                     .reply_cnt = 1 });

                        auto res = R2_PAUSE;

                      } else {

                        ASSERT((*xx->model_cache)[model_id] != nullptr);
                        GetPayload req = {
                          .table_id = 0,
                          .key = key,
                          .model_id = model_id,
                          .model_seq =
                            (*xx->model_cache)[model_id]->train_watermark,
                          .model_key = xx->submodels_addr
                        };
                        char* msg_buf =
                          send_buf + 64; // add a padding to store rpc header

                        Marshal<GetPayload>::serialize_to(req, msg_buf);
                        // LOG(4) << "need to update:" << model_id;
                        // sleep(1);

                        auto ret =
                          rpc.call({ .cor_id = R2_COR_ID(), .dest = addr },
                                   // GET_ID,
                                   PREDICT, // in this context, predict means
                                   // get_w_fallback
                                   { .send_buf = msg_buf,
                                     .len = sizeof(GetPayload),
                                     .reply_buf = reply_buf,
                                     .reply_cnt = 1 });

                        auto res = R2_PAUSE;
                        ASSERT(res == SUCC) << "r2 wait res: " << res
                                            << " in get() fallback path";
                        // break;
                        u32 cur_model_sz = *((u32*)reply_buf);
                        if (cur_model_sz != 0) {
                          Option<u64> ext_addr = {};

                          auto& model = *((*xx->model_cache)[model_id]);
                          model.lock.lock();
                          auto cached_seq = model.seq;
                          model.seq = 0; // invalid

                          r2::compile_fence();
                          auto ret = Serializer::direct_extract(
                            reply_buf + sizeof(u32), model);
                          ASSERT(ret);

                          if (ext_addr) {
                            ASSERT(false) << " current not supported ";
                          }
                          r2::compile_fence();
                          model.seq = cached_seq += 1;
                          r2::compile_fence();
                          model.lock.unlock();
                        } else {
                          // not update
                          // ASSERT(false);
                        }
                      }
                    }
#endif
                  } break;
                  case SearchCode::None:
                    ASSERT(false);
                    break;
                }

                statics[thread_id].increment();
                counter += 1;

                if (FLAGS_id == 1 && thread_id == 0 && i == 0) {
                  if (counter >= 500000) {
                    LOG(3) << "exam results, min: " << report.min
                           << " ; max: " << report.max
                           << " avg: " << report.average;
                    report.clear();
                    counter = 0;
                  }
                }
                R2_YIELD;
              }
              while(true) {
                sleep(1);
              }
              // exit
              R2_STOP(); // stop the scheduler for an elegant stop
              R2_RET;    // call the R2 keyword for return
            });
          }

          // LOG(2) << "sanity check done for thread: " << thread_id;
          // pass
          bar.wait();
          x->model_cache = global_model_cache;

          R2_RET;
        });

        r.run();

        ASSERT(max_key != 0);
        // LOG(2) << "get worker [" << thread_id << "] with max_key: " <<
        // max_key;

        return 0;
    }));
    } // end creating main get() threads

    ASSERT(handlers.size() == num_threads)
      << "this workload reserve two threads for other usage. So please use at "
         "least 3 threads."
      << handlers.size();
    return handlers;
  }
};

}


}
