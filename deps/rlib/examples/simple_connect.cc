#include "rdma_ctrl.hpp"
#include "rnic.hpp"

using namespace rdmaio;

const int tcp_port = 8888;

/**
 * This file shows an exmaple of how to fetch remote mr attributes using RdmaCtrl.
 */
int main() {

  {
    RdmaCtrl ctrl(tcp_port);
    char *test_buffer = new char[64];

    RNic nic({.dev_id = 0,.port_id = 1});
    RDMA_LOG(2) << "nic " << nic.id << " ready: " << nic.ready();

    RDMA_LOG(2) << ctrl.mr_factory.register_mr(73,test_buffer,64,nic);

    RemoteMemory::Attr attr;

    // now we test the fetched MR
    auto ret = RMemoryFactory::fetch_remote_mr(73,
                                               std::make_tuple("localhost",tcp_port),
                                               attr);
    RDMA_ASSERT(ret == SUCC);
    RDMA_LOG(2) << "Check fetched MR: " << attr.key << " " << (const void *)attr.buf
                << "; test buffer: " << (const void *)test_buffer;
    RDMA_ASSERT(attr.buf == (uintptr_t)test_buffer);

    ctrl.mr_factory.deregister_mr(73);
  }
}
