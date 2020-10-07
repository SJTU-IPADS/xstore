#pragma once

#include "rlib/rdma_ctrl.hpp"

#include "r2/src/msg/ud_msg.hpp"
#include "r2/src/rpc/rpc.hpp"

#include "marshal.hpp"
#include "thread.hpp"

#include "../../../server/proto.hpp"

namespace fstore
{

namespace bench
{

using namespace r2;
using namespace r2::rpc;
using namespace rdmaio;
using namespace ::fstore::server;

using Worker = Thread<double>;

class Helper
{
public:
  using ud_msg_ptr = std::shared_ptr<UdAdapter>;
  static ud_msg_ptr create_thread_ud_adapter(RMemoryFactory &mr_factor,
                                             QPFactory &qp_factory,
                                             RNic &nic,
                                             u32 mac_id,
                                             u32 thread_id)
  {

    Addr my_id = {.mac_id = mac_id, .thread_id = thread_id};

    RemoteMemory::Attr local_mr_attr;
    auto ret = mr_factor.fetch_local_mr(thread_id, local_mr_attr);
    ASSERT(ret == SUCC);
    if (thread_id > 24)
      LOG(2) << "fetch mr done";
    bool with_channel = false;
    auto qp = new UDQP(nic,
                       local_mr_attr,
                       with_channel,
                       QPConfig().set_max_send(64).set_max_recv(2048));
    if (thread_id > 24)
      LOG(2) << "create ud done";
    ASSERT(qp->valid());

    UdAdapter *adapter = new UdAdapter(my_id, qp);
    ASSERT(qp_factory.register_ud_qp(thread_id, qp));

    return std::shared_ptr<UdAdapter>(adapter);
  }

  static void send_rc_deconnect(RPC &rpc, const Addr &server_addr, u64 qp_id)
  {
    auto &factory = rpc.get_buf_factory();
    auto send_buf = factory.get_inline();
    Marshal<QPRequest>::serialize_to({.id = qp_id}, send_buf);
    rpc.call({.cor_id = 1, .dest = server_addr},
             ::fstore::server::DELETE_QP,
             {.send_buf = send_buf,
              .len = sizeof(u64),
              .reply_buf = nullptr,
              .reply_cnt = 0});
  }

  static RCQP *create_connect_qp(RMemoryFactory &mr_factory,
                                 QPFactory &qp_factory,
                                 RNic &nic,
                                 u64 qp_id,
                                 u64 local_mr_id,
                                 const RemoteMemory::Attr &remote_mr,
                                 const Addr &server_addr,
                                 RPC &rpc,
                                 RScheduler &coro,
                                 handler_t &h)
  {
    RemoteMemory::Attr local_mr;
    ASSERT(mr_factory.fetch_local_mr(local_mr_id, local_mr) == SUCC);
    // LOG(4) << "thread: " << local_mr_id << " uses mr key:" << local_mr.key
    //<< " remote mr key: " << remote_mr.key;

    auto qp = new RCQP(nic, remote_mr, local_mr, QPConfig());

    auto &factory = rpc.get_buf_factory();
    auto send_buf = factory.alloc(512);

    Marshal<QPAttr>::serialize_to(
        qp->get_attr(),
        Marshal<QPRequest>::serialize_to({.id = qp_id}, send_buf));

    const u64 buf_size = sizeof(u64) + sizeof(QPAttr) + sizeof(64);
    char reply_buf[buf_size];
    auto ret = rpc.call({.cor_id = coro.cur_id(), .dest = server_addr},
                        CREATE_QP,
                        {.send_buf = send_buf,
                         .len = sizeof(QPRequest) + sizeof(QPAttr),
                         .reply_buf = reply_buf,
                         .reply_cnt = 1});
    coro.pause_and_yield(h);

    u64 res = Marshal<u64>::deserialize(reply_buf, 512);
    if (res == SUCC)
    {
      QPAttr attr = Marshal<QPAttr>::deserialize(reply_buf + sizeof(u64),
                                                 512 - sizeof(u64));
      // LOG(4) << "get remote qp attr: " << attr.qpn << "; port id: " <<
      // attr.port_id;
      ASSERT(qp->connect(attr, QPConfig()) == SUCC);
    }
    else
    {
      delete qp;
      qp = nullptr;
    }

    factory.dealloc(send_buf);

    return qp;
  }

  static ServerMeta fetch_server_meta(const Addr &server_addr,
                                      RPC &rpc,
                                      RScheduler &coro,
                                      handler_t &h)
  {

    ServerMeta res;

    auto &factory = rpc.get_buf_factory();
    auto send_buf = factory.alloc(128);

    char reply_buf[64];

    auto ret = rpc.call({.cor_id = coro.cur_id(), .dest = server_addr},
                        META_SERVER,
                        {.send_buf = send_buf,
                         .len = 0,
                         .reply_buf = reply_buf,
                         .reply_cnt = 1});
    coro.pause_and_yield(h);
    factory.dealloc(send_buf);

    res = Marshal<ServerMeta>::deserialize(reply_buf, 64);
    return res;
  }
};

} // namespace bench

} // namespace fstore
