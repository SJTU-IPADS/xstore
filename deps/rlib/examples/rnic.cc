#include "rnic.hpp"
#include "qp_config.hpp"

using namespace rdmaio;

/**
 * This example gives how to query the RNic information using RLib.
 */
int main() {

  RDMA_LOG(2) << "This example prints the RNIC information on this machine.";

  // print all avaliable Nic's name (including their ports)y
  auto res = RNicInfo::query_dev_names();
  RDMA_LOG(2) << "port num: " << res.size();
  for(auto i : res) {
    RDMA_LOG(2) << "get devs: " << i;
  }

  // open an RNic handler, which is used to create QP, register MR, etc
  RNic nic({.dev_id = 0,.port_id = 1});
  RDMA_LOG(2) << "nic " << nic.id << " ready: " << nic.ready();

  // set a configuration of a QP
  QPConfig config;
  config.clear_access_flags().add_access_read();
  RDMA_LOG(2) << config.desc_access_flags();

  return 0;
}
