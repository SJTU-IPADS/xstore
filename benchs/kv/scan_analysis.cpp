/*!
  Analysis the scan behaviro
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

#include "btree_sample.hpp"

#include <limits>
#include <unistd.h>

using namespace fstore;
using namespace fstore::utils;
using namespace fstore::store;
using namespace fstore::sources::tpcc;

using namespace kv;

/*!
  Ouput the real function we want to learn
 */
static void
output_real_dist(Tree& t);

DEFINE_bool(dump, false, "Whether dump the results to file.");

typedef BNaiveStream<u64, u64, BlockPager> BStream;

/*!
  Verify that:
  for page in a B+tree:
    for key in page:
      sc.index.rmi.pick_model_for_key(key) is the same.
 */
bool
verify_page_model(SC& sc, Tree& t)
{
  u64 count(0);
  PageStream stream(t, 0);
  for (stream.begin(); stream.valid(); stream.next()) {
    auto page = stream.value();
    ASSERT(page != nullptr);
    page->sanity_check();

    auto first_key_model_id = sc.index->rmi.pick_model_for_key(page->keys[0]);
    for (uint i = 1; i < page->num_keys; ++i) {
      auto model = sc.index->rmi.pick_model_for_key(page->keys[i]);

      LOG_IF(0, model == first_key_model_id)
        << "one key use [" << first_key_model_id
        << "]; while another key uses: [" << model << "]";
      if (model != first_key_model_id) {
        count += 1;
        ASSERT(model == first_key_model_id + 1);
      }
    }
    if (page->right) {
      auto start_model =
        sc.index->rmi.pick_model_for_key(page->right->start_key());
      ASSERT(start_model == first_key_model_id ||
             start_model == first_key_model_id + 1)
        << "| model : " << first_key_model_id << " | -> "
        << "| model : " << start_model << " | ";
    }
  }
  LOG(2) << "total invalid counts: " << count;
  return true;
}

Leaf*
find_dest_page(Leaf* page, u64 key)
{
  auto cur = page;
  ASSERT(key >= page->start_key());
  while (cur && cur->end_key() < key) {
    cur = cur->right;
  }
  return cur;
}

KeySpanStatics non_key_data;
/*!
    \param key: key of db
    get a predict of pages [page0, page1, ..., page n]
    verify that min_key(page0) <= key <= max_key(page n)
    \return:  page span of keys
*/
u64
verify_key_range(u64 key, SC& sc, u64 tree_min_key, u64 tree_max_key)
{
  auto model = sc.index->get_model(key);
  auto p0 = sc.index->predict_w_model(key, model);
#if 0
  //  auto p1 = sc.index->predict_w_model(key, model + 1);
  auto p2 = sc.index->predict_w_model(key, model == 0 ? 0 : model - 1);

  //  p0.start = std::min(p0.start, p1.start);
  // p0.end = std::max(p0.end, p1.end);

  if (sc.intersect(p0, p2)) {
    p0.start = std::min(p0.start, p2.start);
    p0.end = std::max(p0.end, p2.end);
  }

  non_key_data.add(p0.end - p0.start + 1);
  if (p0.end - p0.start > 110) {
    LOG(4) << "check predicts: " << p0 << ";" << p2
           << "; predicts: " << p0.end - p0.start + 1;
  }
#endif
  //  LOG(4) << "fetch page: " << p0.start << " ~ " << p0.end;

  // verify that the key is within such range
  auto page_start = sc.get_page(p0.start);
  auto page_end = sc.get_page(p0.end);

  ASSERT(page_start != nullptr)
    << "start page error with logic addr: " << p0.start;

  ASSERT(page_end != nullptr) << "end page error with logic addr: " << p0.end;

  page_start->sanity_check();
  page_end->sanity_check();

  if (p0.start != 0) {
    if (page_start->start_key() > key) {
      auto pre_page = page_start->left;
      ASSERT(pre_page != nullptr);
      if (pre_page->end_key() > key) {
#if 0 // debug printout
        for (uint i = 0; i <= p0.start; ++i) {
          auto p = sc.get_page(i);
          LOG(4) << "check key range: " << p->start_key() << " ~ "
                 << p->end_key();
        }
#endif
        ASSERT(pre_page->end_key() <= key) << "start logic addr: " << p0.start
                                           << "predict using model: " << model;
        ASSERT(page_start->start_key() <= key)
          << "page_start start key: " << page_start->start_key()
          << " ; key: " << key;
      } else {
      }
    }
  }

  if (page_end->end_key() < key) {
    auto next = page_end->right;
    ASSERT(next != nullptr);
    if (next->start_key() < key) {
      LOG(4) << "next model: "
             << sc.index->rmi.pick_model_for_key(next->start_key())
             << "; my predict model: " << model << "," << model + 1
             << "; prev end model: "
             << sc.index->rmi.pick_model_for_key(page_end->end_key());
      auto dest = find_dest_page(page_end, key);
      ASSERT(dest != nullptr);
      LOG(4) << "dest model id: "
             << sc.index->rmi.pick_model_for_key(dest->end_key())
             << " of key range: " << dest->start_key() << " ~ "
             << dest->end_key();
    }
    ASSERT(next->start_key() > key)
      << "next start key: " << next->start_key() << "; my key: " << key
      << " logic addr: " << sc.mp->decode_mega_to_entry(p0.end);
  }

  return p0.end - p0.start;
}

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
    } else {
      ASSERT(false) << "workload type not supported: " << FLAGS_workload_type;
    }
    LOG(4) << "total " << fstore::utils::format_value(num_tuples)
           << " loaded, use database: " << FLAGS_workload_type;
  }

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
    // augument
    //auto &model = sc.index->get_lr_model(0);

    LOG(4) << "sc cache train done.";
  }
  LOG(4) << "SC trained done";

  // find the max key
  u64 max_key(0), min_key(std::numeric_limits<u64>::max());
  for (BStream it(t, 0); it.valid(); it.next()) {
    max_key = std::max(max_key, it.key());
    min_key = std::min(min_key, it.key());
  }
  LOG(4) << "The maxinum key of the tree: " << max_key;

  verify_page_model(sc, t);

  LOG(4) << "start verify key range";

  SampleStream sample(t, 0, max_key);

  u64 non_exist_key(0);
#if 0
  //  for (u64 i = 0; i < max_key; i += 100) {
  for (sample.begin(); sample.valid(); sample.next()) {
    auto i = sample.key();
    if (t.get(i) == nullptr) {
      non_exist_key += 1;
      auto gap = verify_key_range(i, sc, min_key, max_key);
      //      ASSERT(gap < 220);
    }
    print_progress(static_cast<double>(i) / static_cast<double>(max_key), i);
  }
#else
  // below this key we have verified
  KeySpanStatics data;
  //  u64 start_key = 609998355;
  //u64 start_key = 3249446632;
  //u64 start_key = 15074753L;
  u64 start_key = 0;
  for (uint i = start_key; i <= max_key; ++i) {
    if (t.get(i) == nullptr) {
      non_exist_key += 1;
      auto gap = verify_key_range(i, sc, min_key, max_key);
      //      ASSERT(gap < 220);
    } else {
      auto predict = sc.get_predict(i);
      data.add(predict.end - predict.start + 1);
    }
    auto progress = static_cast<double>(i) / static_cast<double>(max_key);
    print_progress(progress, i);
  }

  LOG(4) << "existing key search space: " << data.to_str();
  LOG(4) << "non-existing key search space: " << non_key_data.to_str();
#endif

  ASSERT(non_exist_key > 0) << "all key space stored in the tree";
  LOG(4) << "sampled " << non_exist_key << " non-exsist keys";

  LOG(4) << "analysis uses model config: " << model_config << " done";

  return 0;
}
