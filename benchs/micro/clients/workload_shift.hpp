#pragma once

namespace fstore {

namespace bench {

/*!
  A micro-benchmark to evaluate RDMA's raw performance by varing
  number of round-trips used issued by each client.
 */
class Shift {
 public:
  static std::vector<Worker *> bootstrap_all(std::vector<Statics> &statics,
                                             const Addr &server_addr,
                                             const std::string &server_host,
                                             const u64 &server_port,
                                             PBarrier &bar,
                                             u64 num_threads,u64 my_id,u64 concurrency = 1) {

    // first we init the B+Tree buf
    u64 page_area_sz = 8L * 1024 * 1024 * 1024;
    char *page_buf = new char[page_area_sz];
    ASSERT(page_buf != nullptr);
    BlockPager<Leaf>::init(page_buf,page_area_sz);

    // first we populate the global B+Tree for client's emulation
    YCSBLoader::populate(client_sample_tree,  FLAGS_total_accts, 0xdeadbeaf);

    //FLAGS_rdma_rounds = client_sample_tree.depth;

    LOG(4) << "[Workload shift client] #" << my_id << " bootstrap "
           << num_threads << " threads,"
           << "using #roundtrips" << FLAGS_rdma_rounds;

    std::vector<Worker *> handlers;
    for(uint thread_id = 0;thread_id < num_threads;++thread_id) {
      handlers.push_back(
          new Worker([&,thread_id,my_id,server_addr,concurrency,server_port,server_host]() {

                       if(FLAGS_bind_core)
                         CoreBinder::bind(thread_id);

                       Addr addr( {.mac_id = server_addr.mac_id, .thread_id = thread_id});

                       auto all_devices = RNicInfo::query_dev_names();
                       ASSERT(!all_devices.empty()) << "RDMA must be supported.";

                       auto nic_id = VALNic::choose_nic(thread_id);
                       ASSERT(nic_id < all_devices.size()) << "wrong dev id:" << nic_id;
                       RNic nic(all_devices[VALNic::choose_nic(thread_id)]);

                       usize local_buf_sz = 4096 * 4096;
                       char* local_buf = new char[local_buf_sz];
                       assert(local_buf != nullptr);

                       // First we register the memory
                       auto ret = global_rdma_ctrl().
                                  mr_factory.register_mr(thread_id,
                                                         global_memory_region().base_mem_,
                                                         global_memory_region().base_size_,
                                                         nic);
                       ASSERT(ret == SUCC) << "failed to register memory region.";

                       ret = global_rdma_ctrl().mr_factory.register_mr(
                         thread_id + 1024,
                         local_buf,
                         local_buf_sz,
                         // global_memory_region().base_mem_,
                         // global_memory_region().base_size_,
                         nic);
                       ASSERT(ret == SUCC)
                         << "failed to register memory region " << ret
                         << " with id: " << thread_id + 73;

                       u64 remote_connect_id = 0;
                       auto adapter = Helper::create_thread_ud_adapter(global_rdma_ctrl().mr_factory,
                                                                       global_rdma_ctrl().qp_factory,
                                                                       nic,
                                                                       my_id,thread_id);
                       ASSERT(adapter != nullptr) << "failed to create UDAdapter";
                       /**
                        * connect to the server peer, notice that we use the
                        * **thread_id** as the QP_id.
                        */
                       while(adapter->connect(addr,
                                              ::rdmaio::make_id(server_host,server_port),
                                              remote_connect_id) != SUCC) {
                       }

                       /**
                        * Fetch server's MR
                        */
                       RemoteMemory::Attr remote_mr;
                       while(RMemoryFactory::fetch_remote_mr(remote_connect_id,::rdmaio::make_id(server_host,server_port),remote_mr)
                             != SUCC) { }

                       if (FLAGS_id == 1 && thread_id == 0)
                         LOG(4) << "shift client #"  << thread_id << " bootstrap done, "
                                << " will create: [" << concurrency << "] coroutines for execution, use: " << FLAGS_rdma_rounds << " for benchmarking";
                       /**
                        * Start the main loop
                        */
                       u64 thread_random_seed = FLAGS_seed + 73 * thread_id + 0xdeadbeaf * (FLAGS_id + 1);

                       RScheduler r;
                       RPC  rpc(adapter);

                       rpc.spawn_recv(r);
                       u64 qp_id = my_id << 32 | (thread_id + 1);

                       r.spawnr([&](handler_t &h,RScheduler &r) {
                                  auto id  = r.cur_id();

                                  auto ret = rpc.start_handshake(addr,r,h);
                                  ASSERT(ret == SUCC) << "start handshake error: " << ret;

                                  auto server_meta = Helper::fetch_server_meta(addr,rpc,r,h);

                                  // then we create the specificed QP
                                  auto qp = Helper::create_connect_qp(global_rdma_ctrl().mr_factory,
                                                                      global_rdma_ctrl().qp_factory,
                                                                      nic,
                                                                      qp_id,thread_id,remote_mr,
                                                                      addr,rpc,r,h);
                                  ASSERT(qp != nullptr);

                                  RemoteMemory::Attr local_mr;
                                  ASSERT(global_rdma_ctrl()
                                           .mr_factory.fetch_local_mr(
                                             thread_id + 1024, local_mr) ==
                                         SUCC);
                                  qp->local_mem_ = local_mr;

                                  // sanity check QP function and remote memory value

                                  // spawn coroutines for executing RDMA reqs
                                  for(uint i = 0;i < concurrency;++i) {
                                    r.spawnr([&, i, server_meta](
                                               handler_t& h, RScheduler& r) {
                                      char* send_buf = local_buf + i * 4096;
                                      auto id = r.cur_id();

                                      FastRandom rand(thread_random_seed +
                                                      id * 0xddd + 0xaaa);

                                      Timer t;
                                      FlatLatRecorder lats;

#if 0
                                      usize additional_rounds = std::max(
                                        static_cast<u64>(client_sample_tree.depth) -
                                          std::min(static_cast<u64>(
                                                     client_sample_tree.depth),
                                                   FLAGS_rdma_rounds),
                                        static_cast<u64>(1));
#endif
                                      usize additional_rounds = FLAGS_rdma_rounds - std::min(
                                                                                             static_cast<usize>(client_sample_tree.depth), static_cast<usize>(FLAGS_rdma_rounds));
                                      //                                      additional_rounds = 0;
                                      if (thread_id == 0)
                                        LOG(4) << " additional rounds: " << additional_rounds << " client sample depth: " << client_sample_tree.depth << " " <<
                                          static_cast<u64>(client_sample_tree.depth) - FLAGS_rdma_rounds;

                                      u64 sum = 0;

                                      while (running) {
                                        u64 key = rand.rand_number<u64>(0, std::numeric_limits<u64>::max());
#if 1
                                        //auto key = workload.next_key();

                                        sum +=
                                          client_sample_tree.traverse_to_depth(
                                                                               key, FLAGS_rdma_rounds);
                                        for (uint i = 0; i < additional_rounds;
                                             ++i) {
                                          u64 remote_addr =
                                            server_meta.page_addr +
                                            rand.rand_number<u64>(
                                              0,
                                              server_meta.page_area_sz -
                                                (FLAGS_rdma_payload +
                                                 sizeof(u64)));

                                          ret = RdmaFuture::send_wrapper(
                                            r,
                                            qp,
                                            id,
                                            { .op = IBV_WR_RDMA_READ,
                                              .flags = IBV_SEND_SIGNALED,
                                              .len = static_cast<u32>(
                                                FLAGS_rdma_payload),
                                              .wr_id = id },
                                            { .local_buf = send_buf,
                                              .remote_addr = remote_addr,
                                              .imm_data = 0 });
                                          r.pause(h);
                                        }
#endif
                                        // only sample the latency at specific
                                        // coroutine
                                        if (thread_id == 0 && id == 2) {
                                          lats.add_one(t.passed_msec());
                                          t.reset();
                                          statics[thread_id].data.lat =
                                            lats.get_lat();
                                        }
                                        statics[thread_id].increment();
                                        r.yield_to_next(h);
                                      }
                                      LOG(4) << "dummy sum: " << sum;
                                      r.stop_schedule();
                                      routine_ret(h, r);
                                    });
                                  }
                                  bar.wait();

                                  routine_ret(h,r);
                                });
                       r.run();

                       // after run, some cleaning
                       Helper::send_rc_deconnect(rpc,addr,qp_id);
                       rpc.end_handshake(addr);
                       return 0;
                     }));
    }
    return handlers;
  }
};

} // namespace bench

} // namespace fstore
