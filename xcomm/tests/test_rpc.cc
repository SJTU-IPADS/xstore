#include <gtest/gtest.h>

#include "../src/transport/rdma_ring_t.hh"
#include "../../xutils/huge_region.hh"

namespace test {

using namespace r2;
using namespace rdmaio;
using namespace xstore::util;
using namespace xstore::transport;

class SimpleAllocator : public AbsRecvAllocator {
  RMem::raw_ptr_t buf = nullptr;
  usize total_mem = 0;
  mr_key_t key;

  RegAttr mr;

public:
  SimpleAllocator(Arc<RMem> mem, const RegAttr &mr)
      : buf(mem->raw_ptr), total_mem(mem->sz), mr(mr), key(mr.key) {
    // RDMA_LOG(4) << "simple allocator use key: " << key;
  }

  ::r2::Option<std::pair<rmem::RMem::raw_ptr_t, rmem::mr_key_t>>
  alloc_one(const usize &sz) override {
    if (total_mem < sz)
      return {};
    auto ret = buf;
    buf = static_cast<char *>(buf) + sz;
    total_mem -= sz;
    return std::make_pair(ret, key);
  }

  ::rdmaio::Option<std::pair<rmem::RMem::raw_ptr_t, rmem::RegAttr>>
  alloc_one_for_remote(const usize &sz) override {
    if (total_mem < sz)
      return {};
    auto ret = buf;
    buf = static_cast<char *>(buf) + sz;
    total_mem -= sz;
    return std::make_pair(ret, mr);
  }
};

template <typename Nat> Nat align(const Nat &x, const Nat &a) {
  auto r = x % a;
  return r ? (x + a - r) : x;
}


TEST(rpc, basic) {

  RCtrl ctrl(8888);
  RingManager<128> rm(ctrl);

  auto nic = RNic::create(RNicInfo::query_dev_names().at(0)).value();
  RDMA_ASSERT(ctrl.opened_nics.reg(0, nic));

  auto mem_region = HugeRegion::create(64 * 1024 * 1024).value();
  // auto mem_region = DRAMRegion::create(64 * 1024 * 1024).value();
  auto mem = mem_region->convert_to_rmem().value();
  auto handler = RegHandler::create(mem, nic).value();

  auto alloc = Arc<SimpleAllocator>(
      new SimpleAllocator(mem, handler->get_reg_attr().value()));

  const int max_msg_sz = 4096;
  const int ring_sz    = max_msg_sz * 128;
  const int ring_entry = 128;

  auto recv_cq_res = ::rdmaio::qp::Impl::create_cq(nic, 128);
  RDMA_ASSERT(recv_cq_res == IOCode::Ok);
  auto recv_cq = std::get<0>(recv_cq_res.desc);


  auto receiver = RecvFactory<ring_entry, ring_sz, max_msg_sz>::create(
                      rm, std::to_string(0), recv_cq, alloc)
                      .value();

  // spawn for testing
  ctrl.start_daemon();

  RRingTransport<ring_entry,ring_sz, max_msg_sz> t(73, nic, QPConfig(), recv_cq, alloc);
  LOG(4) << "transport init donex !";

  auto res_c = t.connect("localhost:8888", std::to_string(0), 0,
                         QPConfig());
  ASSERT(res_c == IOCode::Ok) << "res_c error: " << res_c.code.name();
  LOG(4) << "transport connect done!";
}

}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
