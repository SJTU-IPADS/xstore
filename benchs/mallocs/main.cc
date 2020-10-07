#include "r2/src/allocator_master.hpp"
#include "mem_region.hpp"

// leverage the reporter in KV to report performance
#include "../kv/reporter.hpp"
#include "thread.hpp"

#include <gflags/gflags.h>

DEFINE_int64(threads,1,"Number of test thread used.");
DEFINE_int64(mem,64,"Memory total used, in sizeof G.");
DEFINE_int64(running_time, 10,"The seconds of running benchmarks.");
DEFINE_string(type,"malloc","Supported evaluation of different malloc");

using namespace kv;
using namespace fstore;
using namespace fstore::utils;
using namespace r2;

using namespace r2::util;

volatile bool running = true;

/*!
  Generate memory of size from [4,3840],
  which is small memory type defined in jemalloc,
  and other popular malloc implementations.
 */
static u64 eval_malloc(FastRandom &rand,Statics &s) {
  u64 sum = 0;
  while(running) {
    u64 size = rand.rand_number(sizeof(u64),3840 - sizeof(u64));
    size = round_up(size,static_cast<u64>(sizeof(u64)));
    assert(size >= sizeof(u64));
    auto ptr = malloc(size);
    sum += *((u64 *)ptr);
    free(ptr);
    s.increment();
  }
  return sum;
}

static u64 eval_jemalloc(FastRandom &rand,Statics &s) {
  u64 sum = 0;
  while(running) {
    while(running) {
      u64 size = rand.rand_number(sizeof(u64),3840 - sizeof(u64));
      size = round_up(size,static_cast<u64>(sizeof(u64)));
      assert(size >= sizeof(u64));
      auto ptr = jemalloc(size);
      sum += *((u64 *)ptr);
      jefree(ptr);
      s.increment();
    }
  }
  return sum;
}

u64 eval_alloc(FastRandom &rand,Statics &s) {

  auto allocator = AllocatorMaster<0>::get_thread_allocator();
  ASSERT(allocator != nullptr);

  u64 sum = 0;
  while(running) {
      u64 size = rand.rand_number(sizeof(u64),3840 - sizeof(u64));
      size = round_up(size,static_cast<u64>(sizeof(u64)));
      assert(size >= sizeof(u64));

      auto ptr = allocator->alloc(size);
      sum += *((u64 *)ptr);
      allocator->free(ptr);
      s.increment();
  }
  return sum;
}

static std::vector<Thread<double> *> bootstrap_threads(std::vector<Statics> &statics) {

  std::vector<Thread<double> *> handlers;

  if(FLAGS_type == "alloc") {
    AllocatorMaster<0>::init(new char[4 * GB],4 * GB);
    //RInit(new char[1 * GB], 1 * GB);
  }

  for(uint i = 0;i < FLAGS_threads;++i) {

    // spawn the test functions
    handlers.push_back(
        new Thread<double>([&,i]() -> double {
                             FastRandom rand(i + 0xdeadbeaf);
                             if(FLAGS_type == "malloc") {
                               if(i == 0)
                                 LOG(4) << "evaluate original glibc's malloc performance.";
                               return eval_malloc(rand,statics[i]);
                             } else if (FLAGS_type == "jemalloc") {
                               if(i == 0)
                                 LOG(4) << "evaluate jemalloc's original malloc.";
                               return eval_jemalloc(rand,statics[i]);
                             } else if (FLAGS_type == "alloc") {
                               if(i == 0)
                                 LOG(4) << "evaluate jemalloc's wrapped malloc";
                               return eval_alloc(rand,statics[i]);
                             }

                             return 0;
                           }));
    handlers[i]->start();
  }

  return handlers;
}

/*!
  Evaluate various malloc's performance
  Here is the conclusion of evaluating different malloc implementations:
  - std::malloc works well, espcially on mac.
  - on Linux: Jemalloc and ssmalloc has comparable performance on micro-benchmarks.
  - The allocator is slightly slower, but fast than linux's standard malloc.
  - All mallocs scale well.
 */
int main(int argc,char **argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  std::vector<Statics>          statics(FLAGS_threads);
  std::vector<Thread<double> *> handlers = bootstrap_threads(statics);

  Reporter::report_thpt(statics,FLAGS_running_time);
  running = false;

  for(auto i : handlers) {
    i->join();
    delete i;
  }
  return 0;
}
