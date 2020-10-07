#pragma once

#include "mem_region.hpp"
#include "thread.hpp"

#include "../latency_calculator.hpp"
//#include "../net_config.hpp"
#include "../statics.hpp"

#include "../../arc/val/cpu.hh"

#include "lib.hpp"

#include "r2/src/futures/rdma_future.hpp"
#include "r2/src/random.hpp"

#include "utils/cc_util.hpp"

namespace fstore {

using namespace server;
using namespace r2;
using namespace r2::util;

using namespace platforms;

DEFINE_uint64(rdma_payload, 8, "Number of RDMA memory to read.");

namespace bench {

RdmaCtrl&
global_rdma_ctrl();
RegionManager&
global_memory_region();

extern volatile bool running;

using Worker = Thread<double>;

class NullRDMAClient
{
public:
  static std::vector<Worker*> bootstrap_all(std::vector<Statics>& statics,
                                            const Addr& server_addr,
                                            const std::string& server_host,
                                            const u64& server_port,
                                            PBarrier& barrier,
                                            u64 num_threads,
                                            u64 my_id,
                                            u64 concurrency = 1)
  {
    LOG(4) << "[NULL RDMA client] #" << my_id << " bootstrap " << num_threads
           << " threads,"
           << "using payload: " << FLAGS_rdma_payload << " bytes";
    std::vector<Worker*> handlers;
    for (uint thread_id = 0; thread_id < num_threads; ++thread_id) {
      handlers.push_back(new Worker([&,
                                     thread_id,
                                     my_id,
                                     server_addr,
                                     concurrency,
                                     server_port,
                                     server_host]() {
                                      if (FLAGS_bind_core)
                                        CoreBinder::bind(thread_id);
                                      //VALBinder::bind(thread_id / VALBinder::core_per_socket(),
                                      //                thread_id % VALBinder::core_per_socket());

        Addr addr({ .mac_id = server_addr.mac_id, .thread_id = thread_id });

        auto all_devices = RNicInfo::query_dev_names();
        ASSERT(!all_devices.empty()) << "RDMA must be supported.";

        auto nic_id = VALNic::choose_nic(thread_id);
        //auto nic_id = 1;
        ASSERT(nic_id < all_devices.size()) << "wrong dev id:" << nic_id;
        RNic nic(all_devices[nic_id]);

        usize local_buf_sz = 4096 * 4096;
        char *local_buf = new char[local_buf_sz];
        assert(local_buf != nullptr);

        // First we register the memory
        auto ret = global_rdma_ctrl().mr_factory.register_mr(
          thread_id,
          //local_buf,
          //local_buf_sz,
          global_memory_region().base_mem_,
          global_memory_region().base_size_,
          nic);

        ASSERT(ret == SUCC) << "failed to register memory region.";
        ret = global_rdma_ctrl().mr_factory.register_mr(
          thread_id + 1024,
          local_buf,
          local_buf_sz,
          //global_memory_region().base_mem_,
          //global_memory_region().base_size_,
          nic);
        ASSERT(ret == SUCC) << "failed to register memory region " << ret << " with id: " << thread_id + 73;

        int remote_connect_id = thread_id;
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
        }

        /**
         * Fetch server's MR
         */
        RemoteMemory::Attr remote_mr;
        while (RMemoryFactory::fetch_remote_mr(
                 remote_connect_id,
                 ::rdmaio::make_id(server_host, server_port),
                 remote_mr) != SUCC) {
          sleep(1);
        }

        if (thread_id == 1) {
          LOG(4) << "null_rdma  client #" << thread_id << " bootstrap done, "
                 << " will create: [" << concurrency
                 << "] coroutines for execution.";
        }
        /**
         * Start the main loop
         */
        u64 thread_random_seed = FLAGS_seed + 73 * thread_id;

        RScheduler r;
        RPC rpc(adapter);

        rpc.spawn_recv(r);
        u64 qp_id = my_id << 32 | (thread_id + 1);

        r.spawnr([&, thread_random_seed, thread_id](handler_t& h,
                                                    RScheduler& r) {
          auto id = r.cur_id();
          auto ret = rpc.start_handshake(addr, r, h);
          ASSERT(ret == SUCC) << "start handshake error: " << ret;

          auto server_meta = Helper::fetch_server_meta(addr, rpc, r, h);
#if 1 // fetch the connect QP from remote
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
#else // using local created QP for just testing
#endif

#if 0
                                  // sanity check QP
                                  char *local_buf = rpc.get_buf_factory().alloc(4096);
                                  ret = RdmaFuture::send_wrapper(r,qp,id,
                                                                 {.op = IBV_WR_RDMA_READ,
                                                                  .flags = IBV_SEND_SIGNALED,
                                                                  .len   = sizeof(u64),
                                                                  .wr_id = id
                                                                 },
                                                                 {.local_buf = local_buf,
                                                                  .remote_addr = 0,
                                                                  .imm_data = 0});
                                  r.pause(h);
                                  ASSERT(*((u64 *)local_buf) == 0xdeadbeaf);
                                  //LOG(4) << "fetched value: " << std::hex << *((u64 *)local_buf);
#endif

          barrier.wait();
          auto& factory = rpc.get_buf_factory();

          // spawn coroutines for executing RDMA reqs
          for (uint i = 0; i < concurrency; ++i) {
            r.spawnr([&, thread_random_seed, qp](handler_t& h, RScheduler& r) {
              auto id = r.cur_id();

              char* send_buf =
                //(char*)AllocatorMaster<>::get_thread_allocator()->alloc(
                //std::max(static_cast<u64>(4096), FLAGS_rdma_payload));
                local_buf + 4096 * i;

              FastRandom rand(thread_random_seed + id * 0xddd + 0xaaa);

              Timer t;
              FlatLatRecorder lats;

              RemoteMemory::Attr local_mr;
              ASSERT(global_rdma_ctrl().mr_factory.fetch_local_mr(thread_id + 1024, local_mr) == SUCC);
              qp->local_mem_ = local_mr;

              while (running) {
#if 1
                u64 remote_addr =
                  server_meta.page_addr +
                  rand.rand_number<u64>(0,
                                        server_meta.page_area_sz -
                                          (FLAGS_rdma_payload + 1024));
                //u64 remote_Addr = rand.rand_number<u64>(0, 1 * GB);

#else
                u64 remote_addr =
                  server_meta.page_addr +
                  rand.rand_number<u64>(0,
                                        server_meta.page_area_sz -
                                          (FLAGS_rdma_payload + sizeof(u64)));
#endif

#if 1 // TODO: remove this block
                ret = RdmaFuture::send_wrapper(
                  r,
                  qp,
                  id,
                  {
                    .op = IBV_WR_RDMA_READ, .flags = IBV_SEND_SIGNALED,
                    //.len = Leaf::value_offset(0),
                    .len = sizeof(Inner),
                    //.len = FLAGS_rdma_payload,
                    .wr_id = id
                  },
                  { .local_buf = send_buf,
                    .remote_addr = remote_addr,
                    .imm_data = 0 });
                r.pause(h);
#endif

#if 0
                ret = RdmaFuture::send_wrapper(
                  r,
                  qp,
                  id,
                  { .op = IBV_WR_RDMA_READ,
                    .flags = IBV_SEND_SIGNALED,
                    .len = static_cast<u32>(FLAGS_rdma_payload),
                    .wr_id = id },
                  { .local_buf = send_buf,
                    .remote_addr = remote_addr,
                    //.remote_addr = (thread_id + 1) * 64 * MB + id * 1024,
                    .imm_data = 0 });
                r.pause(h);
#endif
                if (thread_id == 0 && id == 2) {
                  lats.add_one(t.passed_msec());
                  t.reset();
                  statics[thread_id].data.lat = lats.get_lat();
                }
                statics[thread_id].increment();
                r.yield_to_next(h);
              }
              r.stop_schedule();
              routine_ret(h, r);
            });
          }

          routine_ret(h, r);
        });
        r.run();

        // after run, some cleaning
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
