#pragma once

#include <memory>

#include "common.hpp"
#include "rlib/rdma_ctrl.hpp"
#include "r2/src/msg/ud_msg.hpp"
#include "r2/src/rpc/rpc.hpp"
#include "r2/src/allocator_master.hpp"

namespace pingpong {

using namespace r2;
using namespace rdmaio;
using namespace r2::rpc;
using namespace fstore;

/*!
  constants used by pingping application
*/
const int server_mac_id = 0;
const int mr_id = 73;
const int qp_id = 73;

const int PP_RPC_ID = 4;

class Helpers {
 public:
  /*!
    Create a UDAdapter
    \param nic: RNIC handler opened by the application.
    \param remote_id: remote host:port pair
    \param qp_id: the QPID of the underlying UD
    \param mr_id: the registered memory
    \param tid:   the thread id
   */
  using ud_msg_ptr = std::shared_ptr<UdAdapter>;
  static ud_msg_ptr bootstrap_ud(QPFactory &qp_factory,RNic &nic,
                                 const MacID &remote_id,int qp_id,int mr_id,
                                 int mac_id = server_mac_id,int tid = 0) {
    RemoteMemory::Attr local_mr_attr;
    auto ret = RMemoryFactory::fetch_remote_mr(mr_id,
                                               remote_id,
                                               local_mr_attr);
    auto qp = new UDQP(nic,local_mr_attr,QPConfig().set_max_send(128).set_max_recv(2048));
    auto sqp = new UDQP(nic,local_mr_attr,QPConfig().set_max_send(128).set_max_recv(2048));

    ASSERT(qp->valid());
    UdAdapter *adapter = new UdAdapter({.mac_id = mac_id,.thread_id = tid},sqp,qp);
    ASSERT(qp_factory.register_ud_qp(qp_id,qp));

    return std::shared_ptr<UdAdapter>(adapter);
  }

  static bool register_mem(RMemoryFactory &mr_factory,RNic &nic,char *mem,u64 mem_size,int mr_id) {
    auto ret = mr_factory.register_mr(mr_id,mem,mem_size,nic);
    AllocatorMaster<>::init(mem,mem_size);
    return ret == SUCC;
  }
};

} // end namespace pingpong
