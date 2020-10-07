#include "rlib/rdma_ctrl.hpp"

#include "thread.hpp"
#include "utils/all.hpp"

#include "r2/src/timer.hpp"
#include "reporter.hpp"

#include "data_sources/ycsb/stream.hpp"
#include "data_sources/ycsb/workloads.hpp"

#include "../server/internal/db_traits.hpp"

using namespace fstore::sources::ycsb;

#include <csignal>
#include <gflags/gflags.h>

DEFINE_uint64(running_time, 5, "Number of seconds for the client to run.");
DEFINE_uint64(threads, 1, "Number of client threads used.");
DEFINE_int64(num, 10000000, "Num of accounts loaded for YCSB.");

namespace fstore {

namespace bench {}

}

using namespace fstore::bench;
using namespace fstore;
using namespace fstore::utils;

volatile bool running = true;

/*!
  Evaluate single-node Tree performance (wo network).
 */
int
main(int argc, char** argv)
{

  // rocksdb::port::InstallStackTraceHandler();
  // signal(SIGSEGV, segfault_handler);

  gflags::ParseCommandLineFlags(&argc, &argv, true);

  u64 memory_sz = 12 * GB;
  char* buf = new char[memory_sz];
  BlockPager<Leaf>::init(buf, memory_sz);

  Tree t;
  uint num_tuples(0);

  //  num_tuples = DataLoader::populate_ycsb_hash_db(t, FLAGS_num, 0xdeadbeaf);
  YCSBHashGenereator it(0, FLAGS_num, 0xdeadbeaf);

  for (it.begin(); it.valid(); it.next()) {
    ValType val;
    val.set_meta(it.key());
    t.put(it.key(), val);
    auto v = t.get(it.key());
    ASSERT(v->get_meta() == it.key());
    num_tuples += 1;
  }
  LOG(4) << "total " << num_tuples << " loaded";

  std::vector<Statics> statics(FLAGS_threads);
  std::vector<Thread<double>*> handlers;

  for (uint i = 0; i < FLAGS_threads; ++i) {
    handlers.push_back(new Thread<double>([&, i]() {
      YCSBCWorkload ycsb(FLAGS_num, 0xdeadbeaf + i, true);

      char* reply_buf = new char[4096];
      u64 sum = 0;
      while (running) {
        auto key = ycsb.next_key();
        auto val_p = t.get(key);
        ASSERT(val_p != nullptr);
        sum += val_p->get_meta();

        memcpy(reply_buf, val_p, sizeof(ValType));

        statics[i].increment();
      }
      LOG(4) << "overall sum: " << sum;
      LOG(4) << "overall reply buf: " << *((u64*)reply_buf);
      return 0;
    }));
  }

  for (auto i : handlers)
    i->start();

  Reporter::report_thpt(statics, FLAGS_running_time);
  sleep(4);
  running = false;

  for (auto i : handlers) {
    // LOG(4) << "thread join with :"  << i->join();
    i->join();
    delete i;
  }

  running = false;
  return 0;
}
