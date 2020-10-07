#pragma once

#include "data_sources/nutanix/stream.hpp"
#include "data_sources/tpcc/stream.hpp"
#include "data_sources/nutanix/stream_1.hpp"

#include "data_sources/nutanix/stream_2.hpp"

#include "data_sources/ycsb/stream.hpp"

#include "datastream/text_stream.hpp"

#include "hybrid_store.hpp"
#include "smart_cache.hpp"
#include "stores/naive_tree.hpp"
#include "stores/tree_iters.hpp"

#include "mega_iter.hpp"

//#include "../server/internal/db_traits.hpp"

namespace kv {

using namespace fstore;
using namespace fstore::store;
using namespace fstore::sources::tpcc;
using namespace fstore::sources::ycsb;

/**
 * For all micro workloads, we used u64 as data type for simpliciy.
 */
/* */
typedef BNaiveStream<u64, u64, BlockPager> BStream;
typedef BPageStream<u64, u64, BlockPager> PageStream;
typedef NaiveTree<u64, u64, BlockPager> Tree;
typedef LeafNode<u64, u64, BlockPager> Leaf;
typedef SCache<u64, u64, Leaf, BlockPager> SC;
typedef LearnedRangeIndexSingleKey<u64, u64> LearnedIdx;
typedef HybridLocalStore<Tree, SC, u64, u64> HybridStore;

class DataLoader
{
public:
  /*!
    Load the TPC-C stock databases.
   */
  static u64 populate_tpcc_stock_db(Tree& t, int start_w, int end_w, u64 seed)
  {
    StockGenerator it(start_w, end_w, seed);
    u64 count(0);

    for (it.begin(); it.valid(); it.next())
      t.put(it.key().to_u64(), count++);
    return count;
  }

  static u64 populate_nutanix_0(Tree& t, int num, u64 seed) {
    //NutNaive it(&t,num,seed);
    ::fstore::sources::NutOne<Tree> it(&t,num,seed);
    u64 count(0);

    for (it.begin(); it.valid(); it.next()) {
      auto key = it.key();
      if (count < 10 ) {
        LOG(4) << "populate nut : " << key;
      }
      t.put(key, count++);
    }
    return count;
  }

  static u64 populate_nutanix_1(Tree& t, int num, u64 seed)
  {
    // NutNaive it(&t,num,seed);
    ::fstore::sources::NutTwo<Tree> it(&t, num, seed);
    u64 count(0);

    for (it.begin(); it.valid(); it.next()) {
      auto key = it.key();
      if (count < 10) {
        LOG(4) << "populate nut : " << key;
      }
      t.put(key, count++);
    }
    return count;
  }

  static u64 populate_ycsb_hash_db(Tree& t, int num, u64 seed)
  {

    YCSBHashGenereator it(0, num, seed);
    u64 count(0);

    for (it.begin(); it.valid(); it.next()) {
      t.put(it.key(), count++);
    }
    return count;
  }

  static u64 populate_ycsb_db(Tree& t, int num, u64 seed)
  {

    YCSBGenerator it(0, num);
    u64 count(0);

    for (it.begin(); it.valid(); it.next()) {
      t.put(it.key(), count++);
    }
    return count;
  }

  static int populate_text_db(Tree& t, const std::string& text_file)
  {
    using TI = datastream::TextIter<i64, u64, parse>;
    TI ti(text_file);
    i64 num(0);
    for (ti.begin(); ti.valid(); ti.next()) {
      t.put(ti.key(), ti.value());
      if (num < 12) {
        LOG(4) << "insert key: " << ti.key() << ": " << ti.value();
      }
      num += 1;
    }
    return num;
  }

  static std::tuple<i64, u64> parse(const std::string& line)
  {
    const i64 shift = 1800000000ll;
    std::istringstream ss(line);
    i64 key;
    u64 val;
    ss >> key;
    ss >> val;
    key += shift;
    return std::make_tuple(key, val);
  }
};

}
