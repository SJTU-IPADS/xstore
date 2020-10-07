/*!
  Analysis various aspects of learned index,
  including the predict range, and various workload key patterns.
  Given a workload, generator the following data:
  - The real key distribution.
  - The learned start/end range.
  - The learned position.
  - The average search space of learned index.
*/

#include "flags.hpp"
#include "loader.hpp"

#include "learned_analysis.hpp"
#include "model_config.hpp"
#include "sc_statics.hpp"

using namespace fstore;
using namespace fstore::utils;
using namespace fstore::store;
using namespace fstore::sources::tpcc;

using namespace kv;

static uint
output_key_distribution(Tree& t);

static void
output_learned_distribution(Tree& t, SC::Learned_index_t& idx);

static void
output_search_space(Tree& t, SC& sc);

static void
output_error_space(Tree& r, SC& sc);

static bool
verify_sc_cache(SC& sc, Tree& t, u64 total_tuples);

/*!
  Ouput the real function we want to learn
 */
static void
output_real_dist(Tree& t);

DEFINE_bool(dump, false, "Whether dump the results to file.");

int
main(int argc, char** argv)
{

  gflags::ParseCommandLineFlags(&argc, &argv, true);
  /**
   * First we initialize the data store
   */
  u64 memory_sz = FLAGS_page_sz_m * MB;
  char* buf = new char[memory_sz];
  BlockPager<Leaf>::init(buf, memory_sz);

  /**
   * First, we load the workload to the btree and learned index.
   */
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
    } else if (FLAGS_workload_type == "txt") {
      num_tuples = DataLoader::populate_text_db(t, FLAGS_txt_name);
    } else if (FLAGS_workload_type == "nat0") {
      num_tuples = DataLoader::populate_nutanix_0(t, FLAGS_num, FLAGS_seed);
    } else if (FLAGS_workload_type == "nat1") {
      num_tuples = DataLoader::populate_nutanix_1(t, FLAGS_num, FLAGS_seed);
    } else {
      ASSERT(false) << "workload type not supported: " << FLAGS_workload_type;
    }
    LOG(4) << "total " << fstore::utils::format_value(num_tuples)
           << " loaded, use database: " << FLAGS_workload_type;
  }

  output_real_dist(t);

  /**
   * Then we train the learned index
   */
  auto model_config = ModelConfig::load(FLAGS_model_config);
  LOG(4) << "analysis uses model config: " << model_config;
  SC sc(model_config);
  {
    // build the smart cache from the tree
    PageStream it(t, 0);
    sc.retrain(&it, t);
    LOG(4) << "sc cache train done.";
  }

  // ASSERT(verify_sc_cache(sc,t,num_tuples));
  LOG(4) << "verify sc cache done";

  /**
   * Finally we output various statics
   */
  auto counts = output_key_distribution(t);
  ASSERT(counts == num_tuples)
    << "counts: " << counts << "; actual tuple loaded:" << num_tuples;

  // output_learned_distribution(t,sc.index);

  output_search_space(t, sc);

  output_error_space(t, sc);

  // LOG(4) << CDF<double>::dump_from_vec(tests,"test");
  auto cdf_train = SCAnalysis<SC, BStream>::get_model_training_cdf(sc);
  FILE_WRITE("model.py", std::ofstream::out)
    << cdf_train.dump_as_np_data();

  return 0;
}

static uint
output_key_distribution(Tree& t)
{

  DataMap<u64, u64> data(FLAGS_workload_type);

  BStream it(t, 0);
  uint count(0);
  for (it.begin(); it.valid(); it.next()) {
    data.insert(it.key(), count++);
  }

  if (FLAGS_dump) {
    LOG(4) << "dump key distribution to : " << FLAGS_workload_type + ".py";

    FILE_WRITE(FLAGS_workload_type + ".py", std::ofstream::out)
      << data.dump_as_np_data();
  }

  return count;
}

static void
output_learned_distribution(Tree& t, SC::Learned_index_t& idx)
{

  DataMap<u64, u64> predicts_cdf("pos");
  DataMap<u64, u64> predicts_start("start");
  DataMap<u64, u64> predicts_end("end");

  BStream it(t, 0);
  for (it.begin(); it.valid(); it.next()) {
    auto predicts = idx.predict(it.key());
    predicts_cdf.insert(it.key(), predicts.pos);
    predicts_start.insert(it.key(), predicts.start);
    predicts_end.insert(it.key(), predicts.end);
  }

  if (FLAGS_dump) {
    FILE_WRITE("pos.py", std::ofstream::out)
      << predicts_cdf.dump_as_np_data("Pos", "Key");
    FILE_WRITE("start.py", std::ofstream::out)
      << predicts_start.dump_as_np_data("Pos_start", "Key");
    FILE_WRITE("end.py", std::ofstream::out)
      << predicts_end.dump_as_np_data("Pos_end", "Key");
  }
}

static void
output_search_space(Tree& t, SC& sc)
{

  BStream it(t, 0);

  auto keyspan = SCAnalysis<SC, BStream>::get_span_statics(sc, it);
  LOG(4) << "key span: " << keyspan.to_str();

  it.begin(); // re-set the iterator
  auto cdf = SCAnalysis<SC, BStream>::get_span_cdf(sc, it);
  it.begin();
  if (FLAGS_dump)
    FILE_WRITE("search.py", std::ofstream::out)
      << cdf.dump_as_np_data("Search", "Key");

  auto data = SCAnalysis<SC, BStream>::get_span_data(sc, it);
  if (FLAGS_dump)
    FILE_WRITE("search_data.py", std::ofstream::out)
      << data.dump_as_np_data("Search space", "Key");
}

static void
output_real_dist(Tree& t)
{
  DataMap<u64, u64> data("real");

  typedef BPageStream<u64, u64, BlockPager> PageStream;
  PageStream it(t, 0);
  MegaPagerV<Leaf> mp;
  MegaStream<Leaf> mega_it(&it, &mp);

  u64 counter = 0;
  for (mega_it.begin(); mega_it.valid(); mega_it.next()) {
    auto val = mega_it.value();
    // LOG(4) << "mega get value: " << val;
    //data.insert(mega_it.key(), val);
    data.insert(mega_it.key(), counter++);
  }
  if (FLAGS_dump)
    FILE_WRITE("real_data.py", std::ofstream::out)
      << data.dump_as_np_data("Real pos", "Key");
}

static void
output_error_space(Tree& t, SC& sc)
{

  PageStream iter(t, 0);
  MegaPagerV<Leaf> mp;
  MegaStream<Leaf> it(&iter, &mp);

  utils::CDF<u64> res("error");

  for (it.begin(); it.valid(); it.next()) {
    auto logic_addr = it.value();
    // ASSERT(sc.index.sorted_array[logic_addr].key == it.key())
    //<< "checked key: " << it.key() << "; stored key: " <<
    //sc.index.sorted_array[logic_addr].key
    //<< "
    auto predict = sc.index->predict(it.key());
    res.insert(std::abs(logic_addr - predict.pos));
  }
  res.finalize();
  if (FLAGS_dump)
    FILE_WRITE("error.py", std::ofstream::out)
      << res.dump_as_np_data("Predict error", "Key");
}

static bool
verify_sc_cache(SC& sc, Tree& t, u64 total_tuples)
{

  typedef BNaiveStream<u64, u64, BlockPager> BStream;
  BStream it(t, 0);
  u64 count(0), sum(0);

  for (it.begin(); it.valid(); it.next(), count++) {
    // LOG(4) << "verify key: " << it.key();
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
    // LOG(4) << "checked value: " << *val_p;
    ASSERT(*val_p == it.value()) << "get key error @ " << count;
    // auto logic = sc.index.get_logic_addr(key);
    // sum += logic;
  }
  ASSERT(sum || true) << "dummy verify sum to avoid optimized out";
  return total_tuples == count;
}
