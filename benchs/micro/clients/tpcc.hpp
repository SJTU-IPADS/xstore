// This file is included after the dynamic_v.hpp, so no header is needed

#include "../../../server/tpcc/tpcc_proto.hpp"
#include "../../../src/data_sources/tpcc/schema.hpp"

#define USE_RPC 0

namespace fstore {

using namespace sources::tpcc;

namespace bench {

const constexpr usize no_ratio = 0;
// const constexpr usize get_ratio = 101;
const constexpr usize os_ratio = 100;

static_assert(no_ratio + os_ratio >= 100, "Transaction mix setting error.");

class TPCC
{
  static int non_uniform_random(FastRandom& r, int A, int C, int min, int max)
  {
    return (((r.rand_number(0, A) | r.rand_number(min, max)) + C) %
            (max - min + 1)) +
           min;
  }

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
    LOG(4) << "TPC-C client running ... ";
    std::vector<Worker*> handlers;

    for (usize thread_id = 0; thread_id < num_threads - 1; thread_id += 1) {
      // pass
      // spawn a polling thread for update sc
      handlers.push_back(new Worker([&,
                                     thread_id,
                                     my_id,
                                     server_addr,
                                     concurrency,
                                     server_port,
                                     server_host,
                                     num_threads] {
        // first we build RDMA connections
        Addr addr({ .mac_id = server_addr.mac_id, .thread_id = thread_id });
        Addr insert_addr(
          { .mac_id = server_addr.mac_id, .thread_id = thread_id });

        auto all_devices = RNicInfo::query_dev_names();
        ASSERT(!all_devices.empty()) << "RDMA must be supported.";

        auto nic_id = ::fstore::platforms::VALNic::choose_nic(thread_id);
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
        LOG(4) << "start fetch remote MR for client: #" << thread_id;

        /**
         * Fetch server's MR
         */
        RemoteMemory::Attr remote_mr;
        while (RMemoryFactory::fetch_remote_mr(
                 remote_connect_id,
                 ::rdmaio::make_id(server_host, server_port),
                 remote_mr) != SUCC) {
        }

        LOG(4) << "TPCC client #" << thread_id << " bootstrap done, "
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
          ret = rpc.start_handshake(insert_addr, R2_ASYNC_WAIT2);
          ASSERT(ret == SUCC) << "start handshake error: " << ret;

          // now we use the connect manager for us to create QP
          rdma::SyncCM cm(::rdmaio::make_id(server_host, server_port));
          LOG(4) << "register with qp id: " << qp_id
                 << " for thread: " << thread_id;
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
          ASSERT(qp != nullptr);
          LOG(4) << "create connected qp done";

          // pass
          bar.wait();
          ASSERT(ssc !=
                 nullptr); // if we started, we must initilized the smart cache
          LOG(4) << "server meta fetch done";
          auto server_meta =
            Helper::fetch_server_meta(addr, rpc, R2_ASYNC_WAIT2);

          // TODO: spawn real coroutine functions
          for (uint i = 0; i < concurrency; ++i) {
            FlatLatRecorder lats;

            r.spawnr([&, server_meta](R2_ASYNC) {
              FastRandom rand(0xdeadbeaf + 0xddd * R2_COR_ID() +
                              thread_id * 73);

              auto local_buf =
                (char*)AllocatorMaster<>::get_thread_allocator()->alloc(40960);

              usize counter = 0;
              Timer t;

              while (running) {

                utils::DistReport<usize> report;
                auto ratio = rand.next() % 100;

                if (ratio < no_ratio) {
                  // continue;
                  // if (t.passed_sec() > 2) {
                  //                    continue;
                  //}
                  auto send_buf = local_buf + 64;

                  // we generate the arguments of TPC-C
                  NOArg req;
                  // req.warehouse_id = rand.rand_number<u32>(1, num_threads +
                  // 1);
                  req.warehouse_id = thread_id + 1;
                  req.district_id = rand.rand_number<u32>(1, kMaxDistrictPerW);
                  req.cust_id = static_cast<u32>(
                    non_uniform_random(rand, 1023, 259, 1, kMaxCustomerPerD));
                  req.num_stocks = rand.rand_number(5, 15);
                  std::set<u64> stock_set;
                  for (uint i = 0; i < req.num_stocks; ++i) {

                    u64 sk = 0;
                    u32 item_id = rand.rand_number(1, kNumItems);

                    if (num_threads - 1 == 1 || rand.next() > 1) {
                      // local case
                      u32 sup_ware = req.warehouse_id;
                      sk = stock_key(sup_ware, item_id).to_u64();
                    } else {
                      u32 sup_ware = rand.rand_number<u64>(1, num_threads + 1);
                      sk = stock_key(sup_ware, item_id).to_u64();
                    }
                    if (stock_set.find(sk) != stock_set.end()) {
                      i--;
                      continue;
                    }
                    // LOG(4) << "get sk: " << sk;

                    stock_set.insert(sk);
                    req.stocks[i] = sk;
                  }

                  Marshal<NOArg>::serialize_to(req, send_buf);

                  char reply_buf[64];
                  auto ret = rpc.call({ .cor_id = R2_COR_ID(), .dest = addr },
                                      NO_ID,
                                      { .send_buf = send_buf,
                                        .len = sizeof(NOArg),
                                        .reply_buf = reply_buf,
                                        .reply_cnt = 1 });
                  R2_EXECUTOR.wait_for(1000000000000L);
                  auto res = R2_PAUSE;

                  ASSERT(res == SUCC)
                    << "r2 wait res: " << res << " in TPCC new-order RPC call";

                } else {

                  FOArg req;
                  // auto ware = rand.rand_number<u32>(1, num_threads + 1);
                  auto ware = thread_id + 1;
                  auto d = rand.rand_number<u32>(1, kMaxDistrictPerW);
                  auto c_id =
                    non_uniform_random(rand, 1023, 259, 1, kMaxCustomerPerD);
                  oidx_key key(
                    ware, d, c_id, std::numeric_limits<u32>::max() - 1);

                  req.seek_key = key.v;
                  // LOG(4) << "roll seek key: " << req.seek_key << key.v << " "
                  // <<
                  ///                    " | " << ware << " " <<d << " " <<
                  ///                    c_id;
                  // sleep(1);

                  auto send_buf = local_buf + 64;

                  // now send  the requests

                  auto fetched_sc = (SC*)ssc;
                  r2::compile_fence();
                  FClient fc(qp,
                             fetched_sc,
                             server_meta.page_addr,
                             server_meta.page_area_sz);

#if USE_RPC // RPC's execution

                  Marshal<FOArg>::serialize_to(req, send_buf);

                  char reply_buf[64];
                  auto ret = rpc.call({ .cor_id = R2_COR_ID(), .dest = addr },
                                      FETCH_ORDER,
                                      { .send_buf = send_buf,
                                        .len = sizeof(NOArg),
                                        .reply_buf = reply_buf,
                                        .reply_cnt = 1 });
                  // R2_EXECUTOR.wait_for(1000000000000L);
                  auto res = R2_PAUSE;

                  ASSERT(res == SUCC)
                    << "r2 wait res: " << res << " in TPCC new-order RPC call";

                  ol_key okl(ware, d, 10000000, 0);
                  req.seek_key = okl.v;
                  Marshal<FOArg>::serialize_to(req, send_buf);

                  ret = rpc.call({ .cor_id = R2_COR_ID(), .dest = addr },
                                 FETCH_OLS,
                                 { .send_buf = send_buf,
                                   .len = sizeof(NOArg),
                                   .reply_buf = reply_buf,
                                   .reply_cnt = 1 });
                  // R2_EXECUTOR.wait_for(1000000000000L);
                  res = R2_PAUSE;

                  ASSERT(res == SUCC)
                    << "r2 wait res: " << res << " in TPCC new-order RPC call";
#else
                  // RDMA's case

                  auto predict = fc.get_predict(req.seek_key);
#if 1
                  if (R2_COR_ID() == 2 && thread_id == 4) {
                    auto span = fc.get_page_span(predict);
                    report.add(span.second - span.first + 1);

#if 0
                    if (span.second - span.first > 20 && t.passed_sec() > 2) {
                      //LOG(4) << "predict an extreme error case, k:" <<
//                        req.seek_key << "; predict: " << predict.start << " " << predict.end;
                      //sleep(1);
                    }
#endif

#if 0
                    if(counter % 100 == 0)
                      LOG(4) << "add: "
                             << span.second - span.first +
                                  1;
#endif

                    if (counter % 100000 == 0) {
                      LOG(2) << "exam results, min: " << report.min
                             << " ; max: " << report.max
                             << " avg: " << report.average
                             << " counter: " << counter;
                    }
                    counter += 1;
                  }
#endif

                  auto res = fc.get_addr(
                    req.seek_key, predict, local_buf, R2_ASYNC_WAIT2);

#if 1
                  switch (std::get<0>(res)) {
                    case SearchCode::Ok:
                      break;
                    case SearchCode::Invalid:
                      statics[thread_id].increment2();
                    case SearchCode::Fallback:
                      // LOG(4) << "fallback using sc: " << fetched_sc;
                      // sleep(1);
                      statics[thread_id].increment3();
                    case SearchCode::Unsafe: // XD: I've calarbrated that rarely
                                             // it enters fallback
                    {
                      // LOG(4) << "fallback with SC :" << (SC *)ssc;
                      Marshal<FOArg>::serialize_to(req, send_buf);

                      char reply_buf[512];
                      auto ret =
                        rpc.call({ .cor_id = R2_COR_ID(), .dest = addr },
                                 FETCH_ORDER,
                                 { .send_buf = send_buf,
                                   .len = sizeof(FOArg),
                                   .reply_buf = reply_buf,
                                   .reply_cnt = 1 });

                      // R2_EXECUTOR.wait_for(10000000000L);
                      auto res = R2_PAUSE;
                      ASSERT(res == SUCC)
                        << "r2 wait res: " << res << " in get() fallback path";

                      // collecting the results
                      // ...
                    } break;
                    case SearchCode::None:
                      break;
                      {
                        // not exist
                        // we fake a dummy reads here, otherwise it's unfair to
                        // compare SC to RPC. this is because SC uses two round
                        // trip for one get(). if the first try not get the
                        // record, then it will not issue the second read in
                        // get_addr.
                        u64 addr = rand.next() % server_meta.page_area_sz;
                        ::r2::rdma::SROp op(qp);

                        op.set_payload(local_buf, ValType::get_payload())
                          .set_remote_addr(std::get<1>(res))
                          .set_read();
                        auto ret = op.execute(IBV_SEND_SIGNALED, R2_ASYNC_WAIT);
                        ASSERT(std::get<0>(ret) == SUCC);
                      }
                      break;
                    default:
                      ASSERT(false)
                        << "invalid search code returned: " << std::get<0>(res);
                  }
#endif // end switch

#endif

                  goto ScanEnd;
                  {
                    // the scan
                    auto res_s =
                      fc.seek(req.seek_key, local_buf, R2_ASYNC_WAIT);
                    // auto res = std::make_pair(Fallback,std::make_pair(0,0));

                    switch (std::get<0>(res_s)) {
                      case Ok: {
                        // we fetch all the page back
#if 0
                    bool fetch_res = fc.fetch_value_pages(
                                                          std::make_pair(static_cast<u64>(page_num),
                                                                         page_num + estimated_pages),
                      local_buf,
                      R2_ASYNC_WAIT);
#else
                        bool fetch_res = fc.scan_with_seek(
                          std::get<1>(res_s), 15, local_buf, R2_ASYNC_WAIT);
#endif
                        goto ScanEnd;
                      ErrorEnd:
                        counter +=
                          0; // a dummy statement, like "pass" in python
                      }
                      case Unsafe:
                      case Invalid:
                        // statics[thread_id].increment2();
                      case Fallback:
                        // statics[thread_id].increment3();
                        {
                          // LOG(4) << "req range: " << std::get<0>(range) <<
                          // "->" << std::get<1>(range)
                          //       << "; expected replies: " <<
                          //       expected_replies;
                          // sleep(1);

                          char* send_buf = local_buf + 64;
                          Marshal<FOArg>::serialize_to(req, send_buf);
                          auto ret =
                            rpc.call({ .cor_id = R2_COR_ID(), .dest = addr },
                                     FETCH_OLS,
                                     { .send_buf = send_buf,
                                       .len = sizeof(FOArg),
                                       .reply_buf = local_buf,
                                       .reply_cnt = 1 });
                          ASSERT(ret == SUCC);
                          R2_PAUSE;
                        }
                        break;
                      default:
                        ASSERT(false);
                    }
                  }
                ScanEnd:
                  // the read case
                  // statics[thread_id].increment1();
                  statics[thread_id].increment1();
                }

                if (thread_id == 0 && R2_COR_ID() == 2) {
                  auto lat = t.passed_msec();
                  lats.add_one(lat);
                  t.reset();
                  //statics[thread_id].data.lat = lats.get_lat();
                  statics[thread_id].set_lat(lats.get_lat());
                }
                statics[thread_id].increment();
              }
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
      LOG(4) << "choose nic " << nic_id << " for thread: " << thread_id;

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
      LOG(3) << "SC thread connects to : " << remote_connect_id;
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
#if 0
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

        ssc = (volatile SC*)ModelFetcher::bootstrap_remote_sc(
          0, rpc, addr, qp, R2_ASYNC_WAIT2);
        bar.wait();

        Timer t;
        usize count = 0;
        u64 sum = 0;

        LOG(4) << "SC update thread, running: " << running << " " << count;

        std::map<int, SC*> gc;
        int epoch = 1;
        double update_gap = 1000000.0;

        while (running && true) {
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
          // LOG(4) << "update one sc done: " << count << " -> " << (SC *)ssc;
          t.reset();

          sum += ((SC*)ssc)->get_predict(count).pos;

          r2::compile_fence();
          count += 1;

          if (gc.find(epoch - 3) != gc.end()) {
            auto it = gc.find(epoch - 3);
            delete it->second;
            gc.erase(it);
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
      << "this workload reserve two threads for other usage. So please use "
         "at "
         "least 3 threads.";

    return handlers;
  }
};
} // namespace bench
}
