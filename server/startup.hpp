#pragma once

#include "rlib/rdma_ctrl.hpp"

#include "r2/src/msg/ud_msg.hpp"
#include "r2/src/rpc/rpc.hpp"

#include "controler/handlers.hpp"

#include "mem_region.hpp"

namespace fstore
{

namespace server
{

using namespace r2::rpc;
using namespace rdmaio;

using region_desc_t = std::pair<std::string, u64>;
using region_desc_vec_t = std::vector<region_desc_t>;

DEFINE_bool(use_polling, true, "Whether server use polling to recv message.");

class StartUp
{
public:
  static void register_regions(RegionManager &r, const region_desc_vec_t &descs)
  {
    for (auto &p : descs)
    {
      ASSERT(r.register_region(p.first, p.second) != -1);
    }
  }

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

    bool with_channel = false;
#if R2_SOLICITED
    if (FLAGS_use_polling)
    {
      //      with_channel = true;
    }
#endif
  retry:
    auto qp = new UDQP(nic,
                       local_mr_attr,
                       with_channel,
                       QPConfig().set_max_send(128).set_max_recv(2048));
    ASSERT(qp->valid()) << "create ud adapter error @" << thread_id;

    UdAdapter *adapter = new UdAdapter(my_id, qp);
    ASSERT(qp_factory.register_ud_qp(thread_id, qp));

    return std::shared_ptr<UdAdapter>(adapter);
  }
};

} // end namespace server

} // end namespace fstore
