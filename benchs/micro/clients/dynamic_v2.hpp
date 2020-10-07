// compared with dynamic workload defined in dynamic.hpp, which uses one
// dedicated thread for insertions, dynamic_v2 uses a workload rand approach, so
// that 95% of requests are get while 5% workloads are insert

#include <queue> // std::queue

#include "../../arc/val/net_config.hh"
#include "r2/src/rdma/connect_manager.hpp"
#include "r2/src/channel/channel.hpp"

#include "./fixed_size_set.hpp"

#include "../../../xcli/mod.hh"

namespace fstore {

namespace bench {

const constexpr usize get_ratio = 101;
// const constexpr usize get_ratio = 101;
const constexpr usize insert_ratio = 5;

DEFINE_bool(force_rpc, false, "whether to force use RPC");
DEFINE_bool(uniform, false, "");
DEFINE_int32(get_ratio, 95, "The percet of reads");
DEFINE_int32(model_update_gap, 5, "The period to update the model.");

volatile SC* ssc = nullptr;
std::shared_ptr<XDirectTopLayer> xcache = nullptr;

std::vector<r2::Channel<u32> *> comm_channels;
std::vector<r2::Channel<u32>*> comm_channels_1;

const usize updater_thread_num = 2;

class DynamicClientV2
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

    ASSERT(updater_thread_num >= 1);
    for(uint i = 0;i < num_threads - updater_thread_num;++i) {
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
    for (usize thread_id = 0; thread_id < num_threads - updater_thread_num; thread_id += 1) {
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

        usize local_buf_sz = 4096 * 4096 * 20;
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
          ret = rpc.start_handshake(insert_addr, R2_ASYNC_WAIT2);
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

              char* send_buf = local_buf + 4096 * 1024 * i * 2;
              ASSERT(max_key != 0) << "max key not initialized!";

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

              const int N = 100000;
              ::fstore::FixedVector<N> inserted_keys;
#if 1
              for (uint i = 0; i < N; ++i) {
                u64 key = Hasher::hash(rand.next() %
                                       (FLAGS_num)); // uniform
                inserted_keys.emplace(key);
              }
              utils::ZipFanD zip_generator(
                N, thread_random_seed + R2_COR_ID() * 0xddd + 0xaaa);
#endif

#if 0 // sanity check newly inserted keys
              if (thread_id == 0) {
                for (uint i = 0; i < 12; ++i) {
                  LOG(2) << "vec #1: " << inserted_keys.data[i];
                }

                for (uint i = 0; i < 12; ++i) {
                  LOG(4) << "get newly inserted keys: " << inserted_keys.get_latest(zip_generator.next());
                }
                ASSERT(false);
              }
#endif

              auto xc =
                new XCacheClient(xcache, &rpc, qp, server_meta.page_addr);

              r2::Timer t;
              while (running) {
                auto ratio = rand.next() % 100;
                if (ratio < FLAGS_get_ratio
                    //|| (thread_id != 0) || my_id != 1
                ) {
                  //                  continue;
                  // has to be updated locally because sc can be changed due to
                  // update threads
                  // u64 key = Hasher::hash(rand.next() % FLAGS_num);
#if 0
                  u64 key = Hasher::hash(rand.next() % (FLAGS_num)); // uniform
                  //u64 key = rand.next() % (max_key + insert_count);
                  //ASSERT(false);
#else
                  u64 key = 0;
                  if (FLAGS_uniform) {
                    key = Hasher::hash(rand.next() % (FLAGS_num)); // uniform
                  } else {
                    // use the key in the inserted keys
                    auto zip_n = zip_generator.next();
                    key = inserted_keys.get_latest(zip_n);
                  }
                  //LOG(4) << "get key: "<< key; sleep(1); continue;
#endif

                  // execute XStore logic
#if 0
                  if (R2_COR_ID() == 2 && thread_id == 4) {
                    // TODO: performance counting
                    auto span = fc.get_page_span(predict);
                    report.add(span.second - span.first + 1);

                    if (counter % 1000000 == 0) {
                      LOG(3) << "exam results, min: " << report.min
                             << " ; max: " << report.max
                             << " avg: " << report.average;
                    }
                  }
#endif
                  // default XStore get
                  // LOG(4) << "XStore get key: " << key;
                  SearchCode res = Ok;
                  if (!FLAGS_force_rpc) {
                    //res = xc->get_direct(key, send_buf, R2_ASYNC_WAIT);
                    res = xc->get_direct_possible_non_exist(key, send_buf, R2_ASYNC_WAIT);
                  } else {
                    // force to use RPC
                    res = Fallback;
                  }
                  switch (res) {
                    case SearchCode::Ok:
                      break;
                    case SearchCode::Invalid:
                      //comm_channels[thread_id]->enqueue(xc->core->select_submodel(key)); // the model need update
                      if (0) {
                        // pass
                      }
#if 0
                      if (t.passed_sec() > 50) {

                        GetPayload req = { .table_id = 0, .key = key };
                        char* msg_buf =
                          send_buf + 64; // add a padding to store rpc header

                        Marshal<GetPayload>::serialize_to(req, msg_buf);

                        char reply_buf[512];
                        auto ret =
                          rpc.call({ .cor_id = R2_COR_ID(), .dest = addr },
                                   PREDICT,
                                   { .send_buf = msg_buf,
                                     .len = sizeof(GetPayload),
                                     .reply_buf = reply_buf,
                                     .reply_cnt = 1 });

                        // R2_EXECUTOR.wait_for(10000000000L);
                        auto res = R2_PAUSE;
                        ASSERT(res == SUCC) << "r2 wait res: " << res
                                            << " in get() fallback path";
                      }
#endif
                      //statics[thread_id].increment2();
                      statics[thread_id].increment2();
                  case SearchCode::Unsafe: // XD: I've calarbrated that rarely
                                             // it enters fallback

                    //statics[thread_id].increment2();
                  case SearchCode::Fallback:{
                    auto model = xc->core->select_submodel(key);
                    if (model < xc->core->submodels.size() / 2) {
                      comm_channels[thread_id]->enqueue(model);
                    } else {
                      comm_channels_1[thread_id]->enqueue(model);
                    }
                    statics[thread_id].increment3();

                    {
                      GetPayload req = { .table_id = 0, .key = key };
                      char* msg_buf =
                        send_buf + 64; // add a padding to store rpc header

                      Marshal<GetPayload>::serialize_to(req, msg_buf);

                      char reply_buf[512];
                      auto ret =
                        rpc.call({ .cor_id = R2_COR_ID(), .dest = addr },
                                 GET_ID,
                                 { .send_buf = msg_buf,
                                   .len = sizeof(GetPayload),
                                   .reply_buf = reply_buf,
                                   .reply_cnt = 1 });

                      // R2_EXECUTOR.wait_for(10000000000L);
                      auto res = R2_PAUSE;
                      ASSERT(res == SUCC)
                        << "r2 wait res: " << res << " in get() fallback path";
                    }
                  }
                    break;
                    case SearchCode::None:
#if 0
                      LOG(4) << "zip_n: " << zip_n << " " << key;

                      xc->debug_get(key,send_buf, R2_ASYNC_WAIT);
                      ASSERT(false);
#endif
                      break;
                  }
                  statics[thread_id].increment1();
                } // end processing get
                else {
#if 0
                  if (t.passed_sec() > 20) {
                    continue;
                  }
#endif
                  insert_count += 1;
                  // u64 key = Hasher::hash(rand.next() % (FLAGS_num * 2))
                  // + max_key; u64 key = Hasher::hash(rand.next() %
                  // (FLAGS_num + insert_count));
                  u64 key = rand1.next() % (max_key);
                  //LOG(4) << "insert key: " << key; sleep(1); continue;
                  // u64 key = Hasher::hash(rand.next() % (FLAGS_num));
                  // u64 key = Hasher::hash(rand.next() % 1000000);
                  // auto pos = rand.next() % 20;
                  // u64 key = Hasher::hash(pos);
                  // LOG(4) << "get rand key: " << key << "; at pos: " <<
                  // pos; LOG(4) << "try insert key: " << key;
                  char* msg_buf = send_buf + 64;
                  char reply_buf[128];

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

                  inserted_keys.emplace(key);

                  // statics[thread_id].increment1(); // add to another
                  // counter
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

          // LOG(2) << "sanity check done for thread: " << thread_id;
          R2_RET;
        });

        r.run();

        ASSERT(max_key != 0);
        // LOG(2) << "get worker [" << thread_id << "] with max_key: " <<
        // max_key;

        return 0;
      }));
    } // end creating main get() threads

    // spawn a polling thread for update ssc
    for (uint updater_id = 0; updater_id < updater_thread_num;++updater_id) {
      handlers.push_back(new Worker([&,updater_id,
                                     my_id,
                                     server_addr,
                                     concurrency,
                                     server_port,
                                     server_host,
                                     num_threads] {
        // the last thread is the model update thread
        usize thread_id = num_threads - updater_id - 1;

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
          {
            char* local_buffer =
              (char*)AllocatorMaster<>::get_thread_allocator()->alloc(
                sizeof(u64));
            rdma::SROp op(qp);
            op.set_payload(
              local_buffer,
              sizeof(u64)); // we want read a u64 to the local buffer
            op.set_read().set_remote_addr(
              0); // this is a read, and at remote addr 0
            auto ret = op.execute_sync();     // execute the request in a sync,
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

          if (updater_id == 0) {

            xcache = std::get<1>(retb);

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
            xboot.batch_update_all(
              xcache->submodels, true, send_buf, R2_ASYNC_WAIT);

            // AllocatorMaster<>::get_thread_allocator()->dealloc(send_buf);

#endif

#endif

            LOG(4) << "update xcache done using " << t.passed_msec() / 1000.0
                   << " ms";
          }
          // ASSERT(false);
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
          char *local_buf =
            (char*)AllocatorMaster<>::get_thread_allocator()->alloc(
                                                                    4096 *1024 * num_threads);
          ASSERT(local_buf != nullptr);

          uint start = 0;
          uint end = num_threads - updater_thread_num;
#if 0
          if (updater_id > 1) {
            start = num_threads / 2;
          } else {
            end = num_threads / 2;
          }
#endif
          for (uint i = start; i < end; ++i) {
            R2_EXECUTOR.spawnr([&, i, local_buf](R2_ASYNC) {
                                 r2::Channel<u32> *c = nullptr;
                                 if (updater_id == 0) {
                                   c = comm_channels[i];
                                 } else {
                                   c = comm_channels_1[i];
                                 }
              char* my_buf = local_buf + i * 4096 * 1024;

              Option<int> prev_updated = {};
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
                    xboot.update_submodel(xcache->submodels[ret.value()],
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
    }

    ASSERT(handlers.size() == num_threads)
      << "this workload reserve two threads for other usage. So please use at "
      "least 3 threads." << handlers.size();
    return handlers;
  }
};

}

}
