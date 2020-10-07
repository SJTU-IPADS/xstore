#include "cpptoml/include/cpptoml.h"

#include "common.hpp"
#include "mem_region.hpp"

#include "r2/src/msg/ud_msg.hpp"
#include "r2/src/rpc/rpc.hpp"
#include "r2/src/scheduler.hpp"
#include "rlib/rdma_ctrl.hpp"
#include "utils/all.hpp"

#include "micro/lib.hpp"

namespace {

using namespace fstore;
using namespace fstore::bench;

using namespace rdmaio;
using namespace fstore::utils;
using namespace r2::rpc;

DEFINE_uint64(id,
              73,
              "Master id, should not colid with other server/client ids");
DEFINE_string(client_config, "client.toml", "Client configuration file.");
DEFINE_uint64(nclients, 1, "Number of clients used.");

struct ClientDesc
{
  std::string host = "";
  u32 port = 8888;
  u32 num_thread = 1;
  u32 concurrency = 1;
  u32 id;
};

RegionManager&
global_memory_region()
{
  static RegionManager rm(64 * MB);
  return rm;
}

class Clients
{
  using config_handle_t = std::shared_ptr<cpptoml::table>;

public:
  static std::vector<ClientDesc> parse_cli_config(const std::string& config)
  {
    std::vector<ClientDesc> res;

    auto handler = cpptoml::parse_file(config);
    auto global_config = parse_general_config(handler);

    auto tarr = handler->get_table_array("client");
    for (auto& tab : *tarr) {
      res.push_back(parse_one_client(tab, global_config));
    }
    ASSERT(FLAGS_nclients <= res.size())
      << "total client in config: " << res.size();
    return std::vector<ClientDesc>(res.begin(), res.begin() + FLAGS_nclients);
  }

  static ClientDesc parse_general_config(const config_handle_t& handle)
  {
    ClientDesc res = {
      .host = "",
      .port =
        handle->get_qualified_as<u32>("general_config.port").value_or(8888),
      .num_thread =
        handle->get_qualified_as<u32>("general_config.thread").value_or(1),
      .concurrency =
        handle->get_qualified_as<u32>("general_config.concurrency").value_or(1)
    };
    return res;
  }

  static const u32 invalid_c_id = 0xfffffff;

  static ClientDesc parse_one_client(const config_handle_t& handle,
                                     const ClientDesc& global)
  {
    ClientDesc res = {
      .host = handle->get_as<std::string>("host").value_or(""),
      .port = handle->get_as<u64>("port").value_or(global.port),
      .num_thread = handle->get_as<i32>("thread").value_or(global.num_thread),
      .concurrency =
        handle->get_as<i32>("concurrency").value_or(global.concurrency),
      .id = handle->get_as<u32>("id").value_or(invalid_c_id)
    };
    return res;
  }
};

}

/*!
  The master will rehersal all other clients's workloads
*/
static std::string
cur_time();
int
main(int argc, char** argv)
{

  gflags::ParseCommandLineFlags(&argc, &argv, true);

  auto& rm = global_memory_region();
  rm.make_rdma_heap();

  auto clients = Clients::parse_cli_config(FLAGS_client_config);

  for (auto c : clients)
    LOG(0) << "parse client host:" << c.host;

  auto all_devices = RNicInfo::query_dev_names();
  ASSERT(!all_devices.empty()) << "RDMA must be supported.";

  u64 master_id = 73;

  RNic nic(all_devices[0]);
  RemoteMemory mr(global_memory_region().base_mem_,
                  global_memory_region().base_size_,
                  nic,
                  MemoryFlags());
  bool with_channel = false;
#if R2_SOLICITED
  // with_channel = true;
#endif
  auto qp = new UDQP(nic,
                     mr.get_attr(),
                     with_channel,
                     QPConfig().set_max_send(16).set_max_recv(2048));
  auto adapter = std::shared_ptr<UdAdapter>(
    new UdAdapter({ .mac_id = master_id, .thread_id = 73 }, qp));

  // connect the adapter to remote clients
  for (uint i = 0; i < clients.size(); ++i) {
    ASSERT(clients[i].id >= 0 && clients[i].id != master_id);
    Addr addr({ .mac_id = clients[i].id, .thread_id = controler_id });

    LOG(0) << "connect to client: (" << clients[i].host << ","
           << clients[i].port << ")"
           << "; addr: " << addr.to_str();
    while (adapter->connect(addr,
                            ::rdmaio::make_id(clients[i].host, clients[i].port),
                            controler_id) != SUCC) {
      sleep(1);
    }
  }

  LOG(4) << "connect all clients done";

  RScheduler r;
  RPC rpc(adapter);

  rpc.spawn_recv(r);

  volatile bool started = false;
  r.spawnr([&](handler_t& h, RScheduler& r) {
    LOG(4) << "Master started @" << cur_time();
#if 1
    // start the handshake
    for (uint i = 0; i < clients.size(); ++i) {
      Addr addr({ .mac_id = clients[i].id, .thread_id = controler_id });
      LOG(0) << "handshake to addr: " << addr.to_str();
      auto ret = rpc.start_handshake(addr, r, h);
      ASSERT(ret == SUCC) << "start handshake error: " << ret;
    }
#endif
    LOG(2) << "start handshake with all clients done";

    auto& factory = rpc.get_buf_factory();
    auto send_buf = factory.alloc(128);
    char* reply_buf = new char[clients.size() * sizeof(u64)];

    auto id = r.cur_id();
    // send start RPCs
    for (uint i = 0; i < clients.size(); ++i) {
      Addr addr({ .mac_id = clients[i].id, .thread_id = controler_id });
      auto ret = rpc.call({ .cor_id = id, .dest = addr },
                          START,
                          { .send_buf = send_buf,
                            .len = sizeof(u64),
                            .reply_buf = reply_buf,
                            .reply_cnt = 1 });
      ASSERT(ret == SUCC);
    }
    //r.wait_for(100000000L);
    auto ret = r.pause_and_yield(h);
    ASSERT(ret == SUCC) << "r2 started get error result: " << ret;
    started = true;

    LOG(2) << "send start RPCs to all clients done, and receive their replies.";

    Reports* res = new Reports[clients.size()];
    std::fstream fs("time_spot.py",std::ofstream::out);
    fs << "points = [";

    for (uint e = 0; e < FLAGS_epoch; ++e) {

      sleep(1);
      //usleep(1000);

      for (uint i = 0; i < clients.size(); ++i) {
        Addr addr({ .mac_id = clients[i].id, .thread_id = controler_id });

        auto ret = rpc.call({ .cor_id = id, .dest = addr },
                            PING,
                            { .send_buf = send_buf,
                              .len = sizeof(u64),
                              .reply_buf = (char*)(res),
                              .reply_cnt = 1 });
        ASSERT(ret == SUCC);
      }
      r.pause_and_yield(h);
      double all(0), all_others(0),all_others1(0),all_others2(0),all_lat(0);
      for (uint i = 0; i < clients.size(); ++i) {
        all += res[i].throughpt;
        all_others += res[i].other;
        all_others1 += res[i].other1;
        all_others2 += res[i].other2;
        all_lat += res[i].latency;
      }

      // latency is averaged over all clients
      all_lat = all_lat / clients.size();

      LOG(4) << "at epoch " << e << " thpt: " << format_value(all, 2)
             << " reqs/sec; "
             << "get perf: "<< format_value(all_others,2)
             << ";invalid: " << format_value(all_others1,2)
             << "; fallback: " << format_value(all_others2,2)
             << "lat: " << all_lat << " us";

      if(all_others == 0)
        all_others = 1;
      //ASSERT(all_others1 <= all_others && all_others != 0) << all_others << ;
      fs << "(" <<  all << ","
         << all_others1 / all_others << ","
         << all_others2 / all_others << ","
         << "),";

    }
    fs << "]";
    fs.close();

    LOG(3) << " done!!!";

    // send end RPCs
    for (uint i = 0; i < clients.size(); ++i) {
      Addr addr({ .mac_id = clients[i].id, .thread_id = controler_id });
      auto ret = rpc.call({ .cor_id = id, .dest = addr },
                          END,
                          { .send_buf = send_buf,
                            .len = sizeof(u64),
                            .reply_buf = nullptr,
                            .reply_cnt = 0 });
      ASSERT(ret == SUCC);
    }

    started = false;
    r.stop_schedule();
    routine_ret(h, r);
  });

  // TODO: spawn some coroutine for heartbeat with the server

  r.spawnr([&](R2_ASYNC) {
    while (!started) {
      r2::compile_fence();
      R2_YIELD;
    }

    LOG(4) << "start heartbeat!";

    auto& factory = rpc.get_buf_factory();
    auto send_buf = factory.alloc(128);

    while (started) {
      r2::compile_fence();
      for (uint i = 0; i < clients.size(); ++i) {
        Addr addr({ .mac_id = clients[i].id, .thread_id = controler_id });
        auto ret = rpc.call({ .cor_id = R2_COR_ID(), .dest = addr },
                            BEAT,
                            { .send_buf = send_buf,
                              .len = sizeof(u64),
                              .reply_buf = nullptr, // it's ok, the handler must reply 0 payload
                              .reply_cnt = 1 });
        ASSERT(ret == SUCC);
        R2_EXECUTOR.wait_for(12000000000L);
        auto res = R2_PAUSE;
        ASSERT(res == SUCC) << "error on heartbeat with " << addr.to_str();
      }
      sleep(1);
    }
    R2_RET;
  });

  r.run();

  return 0;
}

static std::string
cur_time()
{
  time_t rawtime;
  struct tm* timeinfo;
  char buffer[64];

  time(&rawtime);
  timeinfo = localtime(&rawtime);

  strftime(buffer, sizeof(buffer), "%d-%m-%Y %H:%M:%S", timeinfo);
  return std::string(buffer);
}
