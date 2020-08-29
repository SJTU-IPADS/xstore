#include <gflags/gflags.h>

#include "../tests/transport_util.hh"

#include "../src/rpc/mod.hh"

#include "../src/transport/rdma_ud_t.hh"

#include "../../deps/r2/src/thread.hh"

namespace bench {

using namespace xstore::rpc;
using namespace xstore::transport;

// prepare the sender transport
using SendTrait = UDTransport;
using RecvTrait = UDRecvTransport<2048>;
using SManager = UDSessionManager<2048>;

// this benchmark simply returns a null reply
void bench_callback(const Header &rpc_header, const MemBlock &args,
                   SendTrait *replyc) {
  // sanity check the requests
  ASSERT(args.sz == sizeof(u64)) << "args sz:" << args.sz;
  auto val = *((u64 *)args.mem_ptr);
  ASSERT(val == 73);
  // LOG(4) << "in tes tcallback !"; sleep(1);

  // send the reply
  RPCOp op;
  char reply_buf[64];
  op.set_msg(MemBlock(reply_buf, 64))
      .set_reply()
      .set_corid(2)
      .add_arg<u64>(73 + 1);
  auto ret = op.execute(replyc);
  ASSERT(ret == IOCode::Ok);
}

RCtrl ctrl(8888);

}

using namespace bench;

DEFINE_int64(threads, 1, "num server thread used");

using XThread = ::r2::Thread<usize>;

int main(int argc, char **argv) {

  gflags::ParseCommandLineFlags(&argc, &argv, true);

  std::vector<std::unique_ptr<XThread>> workers;

  for (uint i = 0; i < FLAGS_threads; ++i) {
    workers.push_back(
        std::move(std::make_unique<XThread>([]() -> usize { return 0; })));
  }
  for (auto &w : workers) {
    w->start();
  }

  for (auto &w : workers) {
    w->join();
  }

  LOG(4) << "rpc server finishes";
  return 0;
}
