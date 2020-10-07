#include "../../src/common.hpp"
#include "../../src/rdma/connect_manager.hpp"
#include "../../src/rdma/single_op.hpp"
#include "../../src/timer.hpp"

#include <gflags/gflags.h>

DEFINE_string(server_host, "localhost", "Server host used.");
DEFINE_int64(server_port, 8888, "Server port used.");
DEFINE_int64(mr_id, 73, "server's mr id. should be equal at server.");

using namespace r2;
using namespace rdmaio;

/*!
    This is an example of using a client to communicate at server
    with one-sided operations.
    It does the following steps:
    - 1. set up local environment (MR,QP)
    - 2. connect QP with remote (by fetching remote MR, QP info)
    - 3. communicate with remote with the connected QP.

    All the API uses R2's frameowork, so there are two versions.
    One async (with coroutine), and one sync (without coroutine).
 */
void
sync_client(RCQP* qp, char* local_buffer);

void
async_client(RCQP* qp, char* local_buffer);

int
main(int argc, char** argv)
{
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  // a local buffer to store for local communication
  char* local_buffer = new char[1024];

  // open the first RDMA nic found
  // Note, we assume that there are RDMA NIC
  usize nic_id = 0; // we use the first nic found
  RNic nic(RNicInfo::query_dev_names()[nic_id]);

  // we register our local buffer to server with the default flags
  RemoteMemory mr(local_buffer, 1024, nic, MemoryFlags());
  ASSERT(mr.valid());

  // Then we fetch the remote MR
  // We use R2's connectmanager for easy setup
  rdma::SyncCM cm(::rdmaio::make_id(FLAGS_server_host, FLAGS_server_port));
  auto res =
    cm.get_mr(FLAGS_mr_id); // this function will retry if failed
  ASSERT(std::get<0>(res) == SUCC);

  // With both remote/local MR, we can create the qp using the default QPConfig
  RCQP* qp = new RCQP(nic, std::get<1>(res), mr.get_attr(), QPConfig());

  // do the connect, similar to TCP connect
  // create a QP at remote and connect to it
  auto ret =
    cm.cc_for_rc(qp,
                 {
                   .qp_id = 12, // the remote QP id for our connection
                   .nic_id = 0, // the remote NIC we want our QP to create at
                   .attr = qp->get_attr(), // remote QP needs to connect
                   .config = QPConfig(),   // we use the default connector
                 },
                 QPConfig());
  ASSERT(ret == SUCC) << "get error ret: " << ret;
  LOG(4) << "all connect done, start testing";

  // finally we sent the results
  sync_client(qp, local_buffer);
  async_client(qp, local_buffer);

  delete[] local_buffer;
  delete qp;
  return 0;
}

void
sync_client(RCQP* qp, char* local_buffer)
{
  u64 sum = 0;
  Timer timer;
  for (uint i = 0; i < 12; ++i) {
    // R2's single RDMA op abstraction
    rdma::SROp op(qp);
    op.set_payload(local_buffer,
                   sizeof(u64));      // we want read a u64 to the local buffer
    op.set_read().set_remote_addr(0); // this is a read, and at remote addr 0
    auto ret =
      op.execute_sync(); // execute the request in a sync, blocking fashion.
    ASSERT(std::get<0>(ret) == SUCC); // check excecute results

    // after execute, the value is stored at local buffer
    sum += *((u64*)local_buffer);
  }
  LOG(2) << "dummy sum: " << sum << "; use " << timer.passed_msec()
         << " microsecond"; // avoids opt by compiler
}

void
async_client(RCQP* qp, char* local_buffer)
{
  RScheduler s; // the async coroutine needs a scheduler/executor
  s.spawnr([&](R2_ASYNC) {
    // The real read function, it is wrapper in a coroutine, so the function
    // must be declared as R2_ASYNC
    // all code are exact the same as sync version, except that the *execute*
    // function
    u64 sum = 0;
    for (uint i = 0; i < 12; ++i) {
      // R2's single RDMA op abstraction
      rdma::SROp op(qp);
      op.set_payload(local_buffer,
                     sizeof(u64)); // we want read a u64 to the local buffer
      op.set_read().set_remote_addr(0); // this is a read, and at remote addr 0

      // Now the execute is a R2_ASYNC functiin
      auto ret = op.execute(R2_ASYNC_WAIT);
      ASSERT(std::get<0>(ret) == SUCC); // check excecute results

      // after execute, the value is stored at local buffer
      sum += *((u64*)local_buffer);
    }
    LOG(2) << "dummy sum: " << sum; // avoids opt by compiler
    R2_STOP();
    R2_RET;
  });

  // run all the coroutines
  s.run();
}
