#include <gtest/gtest.h>

#include "../src/transport/rdma_ring_t.hh"
#include "../../xutils/huge_region.hh"

#if 0

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
  // This test has been abandoned due to trait API changes
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

  /*
    init the recv point at the sender, inorder to receive the reply
   */
  auto recv_cq_res = ::rdmaio::qp::Impl::create_cq(nic, 128);
  RDMA_ASSERT(recv_cq_res == IOCode::Ok);
  auto recv_cq = std::get<0>(recv_cq_res.desc);

  auto receiver = RecvFactory<ring_entry, ring_sz, max_msg_sz>::create(
                      rm, std::to_string(0), recv_cq, alloc)
                      .value();
  /*
    sender's receive entry done
   */

  /*
    init the recv point at the receiver
   */
  recv_cq_res = ::rdmaio::qp::Impl::create_cq(nic, 128);
  RDMA_ASSERT(recv_cq_res == IOCode::Ok);
  auto recv_cq1 = std::get<0>(recv_cq_res.desc);

  auto receiver1 = RecvFactory<ring_entry, ring_sz, max_msg_sz>::create(
                      rm, std::to_string(73), recv_cq1, alloc)
                      .value();

  // spawn for testing
  ctrl.start_daemon();

  RRingTransport<ring_entry,ring_sz, max_msg_sz> t(73, nic, QPConfig(), recv_cq, alloc);
  LOG(4) << "transport init donex !";

  auto res_c = t.connect("localhost:8888", std::to_string(73), 0,
                         QPConfig());
  ASSERT(res_c == IOCode::Ok) << "res_c error: " << res_c.code.name();
  LOG(4) << "transport connect done!";

  // trying to send
  t.send(MemBlock((void *)"okk",4));
  using RingS = RRingTransport<ring_entry, ring_sz, max_msg_sz>::RingS;
  auto wrap =
    Arc<RingS>(t.core, [](RingS *) {});
  receiver->reg_channel(wrap);

  sleep(1);

  int count = 0;
  while (1) {
    // trying to recv at the receive end
    RRingRecvTransport<ring_entry, ring_sz, max_msg_sz> r_end(receiver1);
    for (; r_end.has_msgs(); r_end.next()) {
      auto cur_msg = r_end.cur_msg();
      LOG(0) << "aha, recv one!: " << (char *)cur_msg.mem_ptr << "; msg sz: "
             << cur_msg.sz << "; total: "
             << count << "; check the first:" << (int)((char *)cur_msg.mem_ptr)[0];

      auto s = r_end.reply_entry();
      auto ret = s.send(MemBlock((void *)"ss", 4));
      ASSERT(ret == IOCode::Ok) << ret.desc;
      count += 1;
    }

    if (count > 40960) {
      break;
    }

    // trying to recv at the sender side
    // trying to recv at the receive end
    RRingRecvTransport<ring_entry, ring_sz, max_msg_sz> s_end(receiver);
    for (; s_end.has_msgs(); s_end.next()) {
      auto cur_msg = s_end.cur_msg();
      LOG(0) << "aha, recv one at the sender!: " << (char *)cur_msg.mem_ptr
             << "; msg sz: " << cur_msg.sz << "; total: " << count
             << "; check the first:" << (int)((char *)cur_msg.mem_ptr)[0];

      auto ret = t.send(MemBlock((void *)"okk", 4));
      ASSERT(ret == IOCode::Ok) << ret.desc;
    }
  }
  LOG(4) << "Total:" << count  * 2<< " msgs transfered";
}
}
#endif

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
