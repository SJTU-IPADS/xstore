/*
  A temp routine to bench various NN statistics
 */

#include <gflags/gflags.h>

DEFINE_uint64(nkeys, 100, "Number of keys to load");
DEFINE_uint64(ncheck_model, 1, "Number of models to check");

#define USE_MAP 1
#include "./server/db.hh"

#include "../../../deps/r2/src/timer.hh"

#include "../../../xutils/average_report.hh"
#include "../../../xutils/huge_region.hh"

using namespace rdmaio;
using namespace rdmaio::rmem;
using namespace xstore::util;
using namespace xstore;

int
main(int argc, char** argv)
{

  const usize MB = 1024 * 1024;
  auto mem = HugeRegion::create(128 * 1024 * 1024L).value();

  auto estimated_leaves = FLAGS_nkeys / (kNPageKey / 2) + 16;
  u64 total_sz = sizeof(DBTree::Leaf) * estimated_leaves;
  ASSERT(mem->sz > total_sz) << "total sz needed: " << total_sz;
  xalloc = new XAlloc<sizeof(DBTree::Leaf)>((char*)mem->start_ptr(), total_sz);
  db.init_pre_alloced_leaf(*xalloc);

  // 1. calculate some NN sz
  {
    // the NN is [2,4] X [4,4] X [4,4] X [4,1]
    using Mat = Matrix<float>;
    Mat l0(nullptr, 2, 4);
    Mat b0(nullptr, 1, 4);
    Mat l1(nullptr, 4, 4);
    Mat b1(nullptr, 1, 4);

    Mat l2(nullptr, 4, 4);
    Mat b2(nullptr, 1, 4);

    Mat l3(nullptr, 4, 1);
    Mat b3(nullptr, 1, 1);

    LOG(4) << "sample NN net total uses weights: "
           << l0.payload() + l1.payload() + l2.payload() + l3.payload() << "B";
    LOG(4) << "sample NN net total uses biases: "
           << b0.payload() + b1.payload() + b2.payload() + b3.payload() << "B";
  }

  r2::Timer t;
  LOG(2) << ::xstore::load_map(100) << " Map keys loaded";
  LOG(2) << "load Map dataset in :" << t.passed_msec() << " msecs";

  // then train DB
  {
    r2::Timer t;
    ::xstore::train_db("");
    LOG(2) << "Train dataset in :" << t.passed_msec() << "msecs";
  }

  // bench the predict
  AvgReport<double> report;
  const usize ntraces = 5000;

  std::vector<MapKey> all_keys;
  auto it = DBTreeIter::from(db);
  for (it.begin(); it.has_next(); it.next()) {
    all_keys.push_back(it.cur_key());
  }
  LOG(2) << "all keys sz: " << all_keys.size();

  double sum = 0;
  for (uint i = 0; i < ntraces; ++i) {
    // bench ...
    r2::Timer t;
    sum += cache->get_point_predict(all_keys[i % all_keys.size()]);
    report.add(t.passed_msec());
  }

  // report
  LOG(4) << "min: " << report.min << "; max: " << report.max
         << "; avg: " << report.average << " us";
  LOG(2) << "dummy avoid opt sum: " << sum;

  return 0;
}
