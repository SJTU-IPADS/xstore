#include "hybrid_store.hpp"
#include "learned_analysis.hpp"
#include "model_config.hpp"
#include "sc_statics.hpp"
#include "smart_cache.hpp"
#include "thread.hpp"

#include "utils/all.hpp"

#include "eval_func.hpp"
#include "flags.hpp"
#include "reporter.hpp"

#include "data_sources/tpcc/workloads.hpp"
#include "data_sources/ycsb/workloads.hpp"

#include "data_sources/nutanix/workloads.hpp"

#include "data_sources/txt_workloads.hpp"

#include "r2/src/random.hpp"
#include "r2/src/timer.hpp"

#include <tuple>

/*!
  Evaluate the performance of local KV operation of fstore.
*/

DEFINE_string(tree_type,
              "btree",
              "Supported evaluate types: "
              "[ null, btree, scache, ... ]");
DEFINE_int64(report_batch, 100000, "Report performance per batch operation.");

using namespace fstore;
using namespace fstore::utils;
using namespace fstore::store;
using namespace fstore::sources::tpcc;
using namespace fstore::sources;

using namespace kv;

volatile bool running = true;

// check that the sc cache works as we desires
static bool
verify_sc_cache(SC& sc, Tree& t, u64 total_tuples);

/*!
  A random key space generator for insert workloads
 */
class RandomKeyGenerator
{
  r2::util::FastRandom rand;
  u64 max_key_space;

public:
  explicit RandomKeyGenerator(u64 max_key_space, u64 seed)
    : rand(seed)
    , max_key_space(max_key_space)
  {}

  u64 next_key() { return rand.next() % max_key_space; }
};

template<class T, typename W>
u64
core_eval_func(T& t, W& w, Statics& s)
{
  u64 sum = 0;
  while (running) {
    for (uint i = 0; i < FLAGS_batch_size; ++i) {
      sum += t.eval_func(w.next_key());
      s.increment();
    }
  }
  return sum;
}

template<typename W>
void
sample_workload_cdf(W& w, int num = 10000)
{
  DataMap<u64, u64> workload_samples("workload");
  for (uint i = 0; i < num; ++i)
    workload_samples.insert(i, w.next_key());
  FILE_WRITE("workloads.py", std::ofstream::out)
    << workload_samples.dump_as_np_data();
}

/*!
  \param Workload: The workload generator, which generate key according to a
  distribution. \param Args: Arguments used to initialize the argument.
 */
template<class Workload, typename... Args>
std::vector<Thread<double>*>
bootstrap_threads(Tree& t,
                  SC& sc,
                  LearnedIdx& idx,
                  HybridStore& hs,
                  std::vector<Statics>& statics,
                  u64 num_tuples,
                  Args... args)
{
  std::vector<Thread<double>*> handlers;

  for (uint i = 0; i < FLAGS_threads; ++i) {

    // spawn the test functions
    handlers.push_back(
      /**
       * We need to capture all the variables to the threads.
       * Since we are creating threads inside a function.
       */
      new Thread<double>([&, i, args...]() {
        // LOG(4) << "Eval thread [" << i << "] started!";
        if (i == 0)
          LOG(4) << "sanity check, use workload: " << FLAGS_tree_type;

        Workload w(args..., FLAGS_seed + i);

        if (FLAGS_tree_type == "btree") {

          // The performance of our B+tree
          BTreeTester tt(&t);
          core_eval_func(tt, w, statics[i]);

        } else if (FLAGS_tree_type == "scache") {

          // The performance of smart cache
          SmartCacheTester tt(&sc);
          core_eval_func(tt, w, statics[i]);

        } else if (FLAGS_tree_type == "scache_p") {

          // The performance of predcit, without search the page
          SmartCacheTesterP tt(&sc);
          core_eval_func(tt, w, statics[i]);

        } else if (FLAGS_tree_type == "learn") {

          // The performance of original learned index
          LearnedIndexTester tt(&idx);
          core_eval_func(tt, w, statics[i]);

        } else if (FLAGS_tree_type == "hybrid") {

          HybridStoreTester tt(&hs);
          core_eval_func(tt, w, statics[i]);

        } else if (FLAGS_tree_type == "null") {

          // The performance without doing anything
          NullTester t;
          core_eval_func(t, w, statics[i]);

        } else if (FLAGS_tree_type == "insert") {

          LOG(2) << "insert test started";
          ASSERT(FLAGS_threads == 1) << "raw tree does not support concurrent insert";

          RAWInsertTester tt(&t);
          core_eval_func(tt,w,statics[i]);

        } else if (FLAGS_tree_type == "rtm_insert") {

          RTMInsertTester tt(&t);
          core_eval_func(tt,w,statics[i]);

        } else {
          LOG(4) << "unknown evaluate tree type: " << FLAGS_tree_type << ";"
                 << ", only supports ['btree','scache'].";
        }
        return 73 + i;
      }));
    handlers[i]->start();
  }

  return handlers;
}

int
main(int argc, char** argv)
{

  gflags::ParseCommandLineFlags(&argc, &argv, true);

  // We use B+tree as the default data structures.
  u64 memory_sz = FLAGS_page_sz_m * MB;
  char* buf = new char[memory_sz];
  BlockPager<Leaf>::init(buf, memory_sz);

  // Prepare the B+tree and its data.
  Tree t;
  uint num_tuples(0);
  {
    if (FLAGS_workload_type == "tpcc_stock") {
      num_tuples = DataLoader::populate_tpcc_stock_db(
        t, FLAGS_start_warehouse, FLAGS_end_warehouse, FLAGS_seed);
    } else if (FLAGS_workload_type == "ycsb") {
      num_tuples = DataLoader::populate_ycsb_db(t, FLAGS_num, FLAGS_seed);
    } else if (FLAGS_workload_type == "ycsbh") {
      num_tuples = DataLoader::populate_ycsb_hash_db(t, FLAGS_num, FLAGS_seed);
    } else if (FLAGS_workload_type == "nat0") {
      num_tuples = DataLoader::populate_nutanix_0(t, FLAGS_num, FLAGS_seed);
    } else if (FLAGS_workload_type == "nat1") {
      num_tuples = DataLoader::populate_nutanix_1(t, FLAGS_num, FLAGS_seed);
    } else {
      ASSERT(false) << "workload type not supported: " << FLAGS_workload_type;
    }
  }

  LOG(4) << "total " << fstore::utils::format_value(num_tuples)
         << " loaded, use database: " << FLAGS_workload_type;

  LOG(4) << "total index sz: " << t.node_sz;
  LOG(4) << "total page allocated: " << BlockPager<Leaf>::allocated;
  LOG(4) << "tree height: " << t.depth;

  for(uint d = 1;d <= t.depth;++d)
    LOG(4) << "index sz level #" << d << ": " << t.calculate_index_sz(d) / (1024 * 1024.0) << " MB";

  // lookup
  r2::Timer timer;

  u64 sum = 0;

  YCSBCWorkload workload(FLAGS_num,true,0xdeadbeaf);
#if 0
  usize total_recs = 10000000;
  for(uint i = 0;i < total_recs;++i) {
    //auto v = t.get(workload.next_key());
    auto page = t.find_leaf_page(workload.next_key());
    //ASSERT(v != nullptr);
    ASSERT(page != nullptr);
    sum += page->num_keys;
  }
  LOG(4) << "dummy sum: " << sum;
  LOG(4) << "lookup time: " << timer.passed_msec() / static_cast<double>(total_recs);
#endif
  //return 0;

  // Train the learned index.
  auto model_config = ModelConfig::load(FLAGS_model_config);
  LOG(4) << model_config;

  SC sc(model_config);
  if (FLAGS_tree_type == "scache" || FLAGS_tree_type == "scache_p" ||
      FLAGS_tree_type == "hybrid") {
    // build the smart cache from the tree
    PageStream it(t, 0);
    sc.retrain(&it, t);
    LOG(4) << "sc cache train done.";
  }

  HybridStore hs(&t, &sc);

  LearnedIdx lidx(model_config);
  if (FLAGS_tree_type == "learn") {
    // train the learned index
    BStream all_keys(t, 0);
    for (all_keys.begin(); all_keys.valid(); all_keys.next()) {
      lidx.insert(all_keys.key(), all_keys.value());
    }
    lidx.finish_insert();
    lidx.finish_train();
    LOG(4) << " learned index train done.";
  }
#if 0
  ASSERT(verify_sc_cache(sc,t,num_tuples));
  LOG(4) << "train and verify sc cache done.";
#endif

  std::vector<Statics> statics(FLAGS_threads);
  std::vector<Thread<double>*> handlers;

  // bootstrap all threads
  if (FLAGS_workload_type == "tpcc_stock") {
    handlers = bootstrap_threads<StockWorkload>(t,
                                                sc,
                                                lidx,
                                                hs,
                                                statics,
                                                num_tuples,
                                                FLAGS_start_warehouse,
                                                FLAGS_end_warehouse);
  } else if (FLAGS_workload_type == "ycsbh") {
    handlers = bootstrap_threads<YCSBCWorkload>(
      t, sc, lidx, hs, statics, num_tuples, FLAGS_num, true);
  } else if (FLAGS_workload_type == "dynamic") {
    handlers = bootstrap_threads<RandomKeyGenerator>(
      t, sc, lidx, hs, statics, num_tuples, std::numeric_limits<u64>::max());
    FLAGS_running_time = 10; // donot insert many times
  } else if (FLAGS_workload_type == "nat0") {
    std::vector<u64>* all_keys = new std::vector<u64>();
    std::fstream fs("keys.txt", std::ofstream::in);
    u64 temp;
    while (fs >> temp) {
      all_keys->push_back(temp);
    }
    handlers =
      bootstrap_threads<TXTWorkload>(t, sc, lidx, hs, statics, FLAGS_num, all_keys);
    //handlers = bootstrap_threads<Nut0Workload>(t,sc,lidx,hs,statics,FLAGS_num);
  }

  Reporter::report_thpt(statics, FLAGS_running_time);
  sleep(4);
  running = false;

  for (auto i : handlers) {
    // LOG(4) << "thread join with :"  << i->join();
    i->join();
    delete i;
  }

  return 0;
}

static bool
verify_sc_cache(SC& sc, Tree& t, u64 total_tuples)
{

  typedef BNaiveStream<u64, u64, BlockPager> BStream;
  BStream it(t, 0);
  u64 count(0);

  for (it.begin(); it.valid(); it.next(), count++) {
    auto key = it.key();
    auto val_p = sc.get(key);
    if (unlikely(val_p == nullptr)) {
      LOG(4) << "got a null key: " << key;
      return false;
    }
#if 0
    if(count % 10000 == 0) {
      LOG(4) << "passed: " << count;
    }
#endif
    ASSERT(*val_p == it.value());
  }
  return total_tuples == count;
}
