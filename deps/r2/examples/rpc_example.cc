/**
 * An example of how to use RPC, backed by an UdAdapter.
 *
 */
#include "rlib/rdma_ctrl.hpp"
#include "msg/ud_msg.hpp"
#include "rpc/rpc.hpp"

using namespace r2;
using namespace rdmaio;
using namespace r2::rpc;

RdmaCtrl ctrl(8888);

/**
 * Some helper functions for establishing network communication
 */
char *bootstrap_memory(RNic &nic, int mem_size, int mr_id);
std::shared_ptr<UdAdapter> bootstrap_ud(RNic &nic, int qp_id, int mr_id);

/**
 * The callback we want to run.
 */
void test_callback(RPC &, const Req::Meta &ctx, const char *, void *);

int main()
{
  int mr_id = 73;
  int qp_id = 73;
  int rpc_id = 2;

  auto all_devices = RNicInfo::query_dev_names();
  ASSERT(!all_devices.empty());

  RNic nic(all_devices[0]);
  char *buffer = bootstrap_memory(nic, 1024 * 1024 * 512, mr_id);
  auto adapter = bootstrap_ud(nic, qp_id, mr_id);

  RScheduler r;
  RPC rpc(adapter);
  rpc.register_callback(rpc_id, test_callback);
  rpc.spawn_recv(r);

  r.spawnr([&](handler_t &h, RScheduler &r) {
    auto id = r.cur_id();
    auto factory = rpc.get_buf_factory();

    char *send_buf = factory.alloc(128);
    const char *content = "Hello,";

    strcpy(send_buf, content);
    char reply_buf[64];
    auto ret = rpc.call({.cor_id = id, .dest = {.mac_id = 0, .thread_id = 73}},
                        rpc_id,
                        {.send_buf = send_buf, .len = strlen(content) + 1, .reply_buf = reply_buf, .reply_cnt = 1});
    ASSERT(ret == SUCC);
    LOG(4) << "send done";
    ret = r.pause_and_yield(h);
    ASSERT(ret == SUCC);
    LOG(4) << "receive reply: " << reply_buf;

    r.stop_schedule();
    routine_ret(h, r);
  });

  r.run();
}

char *bootstrap_memory(RNic &nic, int mem_size, int mr_id)
{
  char *buffer = new char[mem_size];
  ASSERT(buffer);
  ASSERT(ctrl.mr_factory.register_mr(mr_id, buffer, mem_size, nic) == SUCC);

  RInit(buffer, mem_size);
  RThreadLocalInit();

  return buffer;
}

std::shared_ptr<UdAdapter> bootstrap_ud(RNic &nic, int qp_id, int mr_id)
{
  RemoteMemory::Attr local_mr_attr;
  auto ret = RMemoryFactory::fetch_remote_mr(mr_id,
                                             std::make_tuple("localhost", 8888),
                                             local_mr_attr);
  auto qp = new UDQP(nic, local_mr_attr, QPConfig().set_max_send(64).set_max_recv(2048));
  ASSERT(qp->valid());
  UdAdapter *adapter = new UdAdapter({.mac_id = 0, .thread_id = 73}, qp);
  ASSERT(ctrl.qp_factory.register_ud_qp(qp_id, qp));

  ASSERT(adapter->connect({.mac_id = 0, .thread_id = 73}, // connect to myself
                          ::rdmaio::make_id("localhost", 8888), qp_id) == SUCC);

  return std::shared_ptr<UdAdapter>(adapter);
}

void test_callback(RPC &rpc, const Req::Meta &ctx, const char *msg, void *)
{
  auto factory = rpc.get_buf_factory();
  LOG(4) << "receive an RPC callback from cor: " << ctx.cor_id
         << "msg: " << msg;
  char *reply_buf = factory.get_inline_buf();
  strcpy(reply_buf, "world");
  ASSERT(rpc.reply(ctx, reply_buf, strlen("world") + 1) == SUCC);
}
