#pragma once

#include "./lib.hpp"
#include "r2/src/rpc/rpc.hpp"

namespace fstore {

namespace bench {

RdmaCtrl&
global_rdma_ctrl();
RegionManager&
global_memory_region();

extern volatile bool running;

using namespace r2::rpc;

class Controler
{
public:
  static void main_loop(std::vector<Statics>& statics,
                        PBarrier& barrier,
                        u64 my_id,
                        u64 unique_id)
  {

    LOG(4) << "controler main loop starts";

    auto all_devices = RNicInfo::query_dev_names();
    ASSERT(!all_devices.empty()) << "RDMA must be supported.";

    RNic nic(all_devices[0]);
    auto ret = global_rdma_ctrl().mr_factory.register_mr(
      unique_id,
      global_memory_region().base_mem_,
      global_memory_region().base_size_,
      nic);

    auto adapter =
      Helper::create_thread_ud_adapter(global_rdma_ctrl().mr_factory,
                                       global_rdma_ctrl().qp_factory,
                                       nic,
                                       my_id,
                                       unique_id);

    /**
     * Start the main loop
     */
    RScheduler r;
    RPC rpc(adapter);
    rpc.register_callback(
      START, [&](RPC& rpc, const Req::Meta& ctx, const char* msg, u32 payload) {
        // TODO: make some sanity check ?
        LOG(2) << "receive start RPC";
        if (running == false) {
          running = true;
          barrier.wait();
        }
        auto& factory = rpc.get_buf_factory();
        char* reply_buf = factory.get_inline();
        rpc.reply_async(ctx, reply_buf, sizeof(u64));
      });

    std::vector<Statics> old_statics(statics.size());
    Timer t;
    rpc.register_callback(
      PING, [&](RPC& rpc, const Req::Meta& ctx, const char* msg, u32 payload) {
        u64 sum = 0;
        u64 sum1 = 0;
        // now report the throughput
        for (uint i = 0; i < statics.size(); ++i) {
          auto temp = statics[i].data.counter;
          sum += (temp - old_statics[i].data.counter);
          old_statics[i].data.counter = temp;

          temp = statics[i].data.counter1;
          sum1 += (temp - old_statics[i].data.counter1);
          old_statics[i].data.counter1 = temp;
        }

        double passed_msec = t.passed_msec();

        double res = static_cast<double>(sum) / passed_msec * 1000000.0;
        double res1 = static_cast<double>(sum1) / passed_msec * 1000000.0;
        t.reset();

        auto& factory = rpc.get_buf_factory();
        char* reply_buf = factory.get_inline();

        Marshal<Reports>::serialize_to(
            { .throughpt = res, .other = res1, .other1 = 0,.other2 = 0,.latency = statics[0].data.lat },
          reply_buf);
        LOG(0) << "Reply with throughput " << format_value(res, 2)
               << " reqs/sec";
        rpc.reply_async(ctx, reply_buf, sizeof(Reports));
      });

    rpc.register_callback(
      END, [&](RPC& rpc, const Req::Meta& ctx, const char* msg, u32 payload) {
        // TODO: make some sanity check ?
        running = false;
        r.stop_schedule();
      });

    // handle heartbeat
    rpc.register_callback(
      BEAT, [&](RPC& rpc, const Req::Meta& ctx, const char* msg, u32 payload) {
        char reply[64];
        rpc.reply_async(ctx, reply + 32, 0);
      });

    rpc.spawn_recv(r);

    LOG(4) << "controler start listening";
    r.run();
  }
};

} // bench

} // fstore
