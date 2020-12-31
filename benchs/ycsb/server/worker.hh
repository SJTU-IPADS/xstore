#pragma once

#include "../../../deps/rlib/core/lib.hh"

#include "../../../deps/r2/src/thread.hh"

#include "../../../xutils/local_barrier.hh"

#include "./callbacks.hh"

#include "../../../xcomm/tests/transport_util.hh"

extern volatile bool running;
extern volatile bool init;
extern ::rdmaio::RCtrl ctrl;

extern ::xstore::util::PBarrier *bar;

namespace xstore {

using namespace test;

using SendTrait = UDTransport;
using RecvTrait = UDRecvTransport<2048>;
using SManager = UDSessionManager<2048>;

using XThread = ::r2::Thread<usize>;

auto bootstrap_workers(const usize &nthreads)
    -> std::vector<std::unique_ptr<XThread>> {
  std::vector<std::unique_ptr<XThread>> res;

  for (uint i = 0; i < nthreads; ++i) {
    res.push_back(std::move(std::make_unique<XThread>([i]() -> usize {
      auto thread_id = i;
      auto nic_for_recv =
          RNic::create(RNicInfo::query_dev_names().at(0)).value();
      auto qp_recv = UD::create(nic_for_recv, QPConfig()).value();

      // some bootstrap code
      // prepare UD recv buffer
      auto mem_region = HugeRegion::create(64 * 1024 * 1024).value();
      auto mem = mem_region->convert_to_rmem().value();

      auto handler = RegHandler::create(mem, nic_for_recv).value();
      SimpleAllocator alloc(mem, handler->get_reg_attr().value());

      auto recv_rs_at_recv =
          RecvEntriesFactory<SimpleAllocator, 2048, 4096>::create(alloc);
      {
        auto res = qp_recv->post_recvs(*recv_rs_at_recv, 2048);
        RDMA_ASSERT(res == IOCode::Ok);
      }

      ctrl.registered_qps.reg("b" + std::to_string(thread_id), qp_recv);
      LOG(4) << "server thread #" << thread_id << " started!";

      RPCCore<SendTrait, RecvTrait, SManager> rpc(12);

      UDRecvTransport<2048> recv(qp_recv, recv_rs_at_recv);

      // register the callbacks before enter the main loop
      ASSERT(rpc.reg_callback(meta_callback) == META);
      ASSERT(rpc.reg_callback(get_callback) == GET);
      r2::compile_fence();

      bar->wait();
      while(!init) {
        r2::compile_fence();
      }

      usize epoches = 0;
      while (running) {
        r2::compile_fence();
        rpc.recv_event_loop(&recv);
      }

      return 0;
    })));
  }
  return std::move(res);
}

} // namespace xstore
