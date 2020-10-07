#pragma once

#include <gflags/gflags.h>

#include "thread.hpp"

#include "../benchs/arc/val/net_config.hh"
#include "../benchs/arc/val/cpu.hh"
#include "r2/src/rdma/connect_handlers.hpp"
#include "r2/src/channel/channel.hpp"

#include "r2/src/timer.hpp"

#include "startup.hpp"

#include "controler/all.hpp"
#include "tpcc/tpcc_handlers.hpp"

namespace fstore
{

namespace server
{

thread_local RCQP *self_qp = nullptr;

RdmaCtrl &
global_rdma_ctrl();
RegionManager &
global_memory_region();

::r2::rdma::NicRegister&
global_nics();

DECLARE_string(client);

using ServerWorker = Thread<double>;

std::mutex init_lock;

  __thread r2::Channel<u32> *update_channel;
  __thread r2::Channel<u32>* update_channel_1;
  __thread r2::Channel<u32>* update_channel_2;

  std::vector<r2::Channel<u32> *> all_channels;
  std::vector<r2::Channel<u32>*> all_channels_1;
  std::vector<r2::Channel<u32>*> all_channels_2;

  extern __thread u64 insert_count;
  extern __thread u64 invalid_count;

  extern __thread u64 insert_count;
  extern __thread u64 invalid_count;

  std::vector<u64 *> all_inserts;
  std::vector<u64 *> all_invalids;

  class Workers
  {
  public:
    /*!
      The worker thread's implementation at every server.
    */
    static std::vector<ServerWorker*> bootstrap_all(int num_threads,
                                                    u32 my_id,
                                                    PBarrier& bar)
    {
      std::vector<ServerWorker*> handlers;
      for (uint i = 0; i < num_threads; ++i) {
        all_channels.push_back(new r2::Channel<u32>(512));
        all_channels_1.push_back(new r2::Channel<u32>(512));
        all_channels_2.push_back(new r2::Channel<u32>(512));
      }

      for (uint i = 0; i < num_threads; ++i) {
        handlers.push_back(new ServerWorker([i, my_id, &bar]() {
          // CoreBinder::bind(i);
          using namespace ::fstore::platforms;
          VALBinder::bind(i / VALBinder::core_per_socket(),
                          i % VALBinder::core_per_socket());

          update_channel = all_channels[i];
          update_channel_1 = all_channels_1[i];
          update_channel_2 = all_channels_2[i];

          init_lock.lock();
          all_inserts.push_back(&insert_count);
          all_invalids.push_back(&invalid_count);

          auto all_devices = RNicInfo::query_dev_names();
          ASSERT(!all_devices.empty()) << "RDMA must be supported.";

          auto nic_id = ::fstore::platforms::VALNic::choose_nic(i);
          // usize nic_id = 1;
          // LOG(4) << "server thread: " << i << " choose nic: " << nic_id;
          ASSERT(nic_id < all_devices.size()) << "wrong dev id:" << nic_id;

          LOG(4) << "Server: " << i << " use nic id: "<< nic_id;
          RNic nic(all_devices[nic_id]);
          global_nics().reg(i, &nic);

          // First we register the memory
          auto ret = global_rdma_ctrl().mr_factory.register_mr(
            i,
            global_memory_region().base_mem_,
            global_memory_region().base_size_,
            nic);

          ASSERT(ret == SUCC)
            << "failed to register memory region with status: " << ret;

          RemoteMemory::Attr mr;
          ASSERT(global_rdma_ctrl().mr_factory.fetch_local_mr(i, mr) == SUCC);

          auto adapter =
            StartUp::create_thread_ud_adapter(global_rdma_ctrl().mr_factory,
                                              global_rdma_ctrl().qp_factory,
                                              nic,
                                              my_id,
                                              i);
          ASSERT(adapter != nullptr) << "failed to create UDAdapter";

          init_lock.unlock();

          RPC rpc(adapter);

          std::fstream fs("cpu.txt", std::ofstream::out);

          /**
           * Start the main loop
           */
          RScheduler r([&rpc, i, &fs](handler_t& yield, RScheduler& coro) {
            auto thread_id = i;

            r2::Timer t_total;
            r2::Timer t_monitor;

            double time_processed = 0;
            u64 rdtsc_processed = 0;
            u64 rdtsc_start = read_tsc();

            // std::vector<double> cpu_utis;

            // while (coro.is_running()) {
            while (true) {
              // poll the completion events
              if (thread_id != 0) {
                coro.poll_all();
              } else {
                coro.poll_all();
                r2::compile_fence();
                rdtsc_processed += rpc.report_and_reset_processed();
              }

              if (coro.next_id() != coro.cur_id()) {
                coro.yield_to_next(yield);
              }

              double report_gap = 1000000;
              // double report_gap = 100000;

              // only report process at timer 0
              if (thread_id == 0) {
                if (unlikely(t_monitor.passed_msec() > report_gap)) {
                  // print the data
                  // auto total_time_msec = t_total.passed_msec();
                  auto total_tsc = read_tsc() - rdtsc_start;
                  double uti = (double)(rdtsc_processed) / total_tsc;
                  rdtsc_start = read_tsc();
                  rdtsc_processed = 0;

#if 1
                  LOG(0) << "cpu utilization: " << uti * 100
                         << " % time is used to process requests";
                //cpu_utis.push_back(uti * 100);
#else
                  fs << uti * 100 << "\n";
                  fs.flush();
#endif

                  t_monitor.reset();
                }
              }
            }

            routine_ret(yield, coro);
          });

          Controler::register_all(rpc);

          if (0) { // fixme: hard coded now
            TPCCHandlers::register_all(rpc);
          };

          // create a self-QP for local usage
          self_qp = new rdmaio::RCQP(nic, mr, mr, QPConfig());
          self_qp->connect(self_qp->get_attr(), QPConfig());

          /**
           * we registe a specific RPC handler to create QP for us
           */
          rpc.register_callback(
            CREATE_QP,
            [&nic, i, mr](
              RPC& rr, const Req::Meta& ctx, const char* msg, u32 payload) {
              // LOG(4) << "reply one QP start";
              auto& factory = rr.get_buf_factory();
              char* reply_buf =
                factory.alloc(sizeof(u64) + sizeof(QPAttr) + 1024);
              QPRequest req = Marshal<QPRequest>::deserialize(msg, payload);

              IOStatus ret = SUCC;
              if (global_rdma_ctrl().qp_factory.get_rc_qp(req.id) != nullptr)
                ret = REPEAT_CREATE;

              // TODO: maybe we need to be more configurable, currently we use
              // default setting
              auto qp = new rdmaio::RCQP(nic, mr, mr, QPConfig());
              ASSERT(qp != nullptr);
              if (!global_rdma_ctrl().qp_factory.register_rc_qp(req.id, qp)) {
                ret = REPEAT_CREATE;
                // delete qp;
              } else {
                // send the reply back
                QPAttr attr = Marshal<QPAttr>::deserialize(
                  msg + sizeof(QPRequest), payload - sizeof(QPRequest));
                ret = qp->connect(attr, QPConfig());
                Marshal<QPAttr>::serialize_to(qp->get_attr(),
                                              reply_buf + sizeof(u64));
              }
              r2::compile_fence();
              Marshal<u64>::serialize_to(ret, reply_buf);
              rr.reply(ctx, reply_buf, sizeof(u64) + sizeof(QPAttr));

              // usleep(400);
              // LOG(4) << "reply one QP done";
              // factory.dealloc(reply_buf); // we can safely dealloc here, the
              // msg has been flushed
            });

          rpc.spawn_recv(r);

          bar.wait();
          //LOG(4) << "server #thread " << i << " started";

          r.run();
          return 0;
        }));
    }
    return handlers;
  }
};

} // namespace server

} // namespace fstore
