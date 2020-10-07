#include <gflags/gflags.h>

#include "config.hpp"
#include "page_monitor.hpp"

#include "worker.hpp"

#include "internal/learner.hpp"

#include "mem_region.hpp"

#include "r2/src/rdma/connect_handlers.hpp"

#include "loaders/loader.hpp"

#define ROCKSDB_BACKTRACE
//#include "rocksdbb/port/stack_trace.h"

#include "tpcc/bootstrap.hpp"

#include "internal/xcache_learner.hh"

using namespace fstore;
using namespace fstore::server;

namespace fstore {

namespace server {

using namespace rdmaio;
using namespace r2;

DEFINE_int64(threads, 1, "Number of workers used for server.");
DEFINE_uint32(id, 0, "Server id.");
DEFINE_int64(port, 8888, "Server port used.");
DEFINE_string(host, "localhost", "Server host used.");
DEFINE_string(config_file,
              "server/config.toml",
              "Server configuration file used.");
DEFINE_string(model_config,
              "learned.toml",
              "The learned index model used for smart cache.");

DEFINE_string(osm, "osm_uni_100m.txt", "The OSM key file");

DEFINE_string(db_type, "ycsb", "Initialize DB type.");

DEFINE_int64(seed, 0xdeadbeaf, "The random seed used at server.");

// just use for test
DEFINE_string(client, "val00", "Test client id.");

DEFINE_bool(dump_keys,false,"whether to dump all keys to a txt file");

DEFINE_bool(no_train, false, "Whether to pre-train the KV.");


u64 global_mem_sz = 0;

Tables global_table;

RdmaCtrl&
global_rdma_ctrl()
{
  //static RdmaCtrl ctrl(FLAGS_port);
  static RdmaCtrl ctrl;
  return ctrl;
}


RegionManager&
global_memory_region()
{
  static RegionManager rm((char*)alloc_huge_page(global_mem_sz, 2 * MB),
                          global_mem_sz);
  assert(global_mem_sz != 0);
  //static RegionManager rm(global_mem_sz);
  return rm;
}

Config&
global_config()
{
  static Config config(FLAGS_config_file);
  return config;
}

::r2::rdma::NicRegister&
global_nics() {
  static r2::rdma::NicRegister nics;
  return nics;
}

// just for test
//TestLidx test_lidx(ModelConfig::load(FLAGS_model_config));

void
load_db()
{
#if 1
  auto type_map = Loader::get_type_map();
  if (type_map.find(FLAGS_db_type) == type_map.end()) {
    type_map[FLAGS_db_type] = NONE;
  }
  switch (type_map[FLAGS_db_type]) {

    case YCSB: {
      auto id = global_table.register_table("ycsb", FLAGS_model_config);
      assert(id == 0);
      YCSBLoader::populate(
        global_table.get_table(0).data, FLAGS_ycsb_num, FLAGS_seed);
    } break;
    case YCSBH: {
      auto id = global_table.register_table("ycsbh", FLAGS_model_config);
      assert(id == 0);
      YCSBLoader::populate_hash(
        global_table.get_table(0).data, FLAGS_ycsb_num, FLAGS_seed);
    } break;
    case DUMMY: {
      auto id = global_table.register_table("ycsbh", FLAGS_model_config);
      auto id1 = global_table.register_table("dual_test", FLAGS_model_config);

      assert(id == 0);

      // we load a partial db here
      YCSBLoader::populate_hash(
        global_table.get_table(id).data,
        //std::min(static_cast<i64>(1000000L), FLAGS_ycsb_num),
        FLAGS_ycsb_num,
        //10000000,
        //std::min(static_cast<i64>(200), FLAGS_ycsb_num),
        FLAGS_seed);

    } break;
  case TPCC: {
    auto id = global_table.register_table("oidx", FLAGS_model_config);
    TPCCBoot::populate(global_table.get_table(id).data);
  }
  case NUTO: {
    auto id = global_table.register_table("ycsbh", FLAGS_model_config);
    assert(id == 0);
#if 0
    NutLoader::populate(
      global_table.get_table(0).data, FLAGS_ycsb_num, FLAGS_seed);
#else

    NutLoader::populate_simple(
      global_table.get_table(0).data, FLAGS_ycsb_num, FLAGS_seed);

#endif

  }

    break;

  case OSM: {
    auto id = global_table.register_table("osm", FLAGS_model_config);
    OSMLoader::populuate(global_table.get_table(0).data, FLAGS_ycsb_num, FLAGS_osm,0xdeadbeaf);


    break;
  }
    default:
      assert(false);
  };

  LOG(4) << "B+tree load done, leaf sz: " << sizeof(Leaf) << " rdma base: " << (u64)(global_memory_region().base_mem_);
#if 1
  if (FLAGS_dump_keys) {
    typedef BNaiveStream<u64,ValType, BlockPager> BStream;
    BStream it(global_table.get_table(0).data, 0);
    std::fstream fs("keys.txt", std::ofstream::out);
    for (it.begin(); it.valid(); it.next()) {
      fs << it.key() << "\n";
    }
    fs.close();
  }
#endif

  if (!FLAGS_no_train) {
    // train and prepare the db
    Timer t;
    LOG(4) << "start training!";
    XLearner::train_x(global_table.get_table(0), FLAGS_model_config);
    LOG(4) << "train done " << "use: " << t.passed_msec() << " msec";

#if 0 // some simple check
    auto &tab = global_table.get_table(0);
    LOG(4) << "use " << tab.xcache->select_submodel(242699015) << " for prediction!!!";
    auto &model = tab.xcache->sub_models[tab.xcache->select_submodel(242699015)];
    LOG(4) << "sanity check model: " << model.min_error << " " << model.max_error << " " << model.ml.predict(242699015);
#endif

    auto mem = XLearner::serialize_x(global_table.get_table(0));
    LOG(4) << "xcache model num: " << std::get<0>(mem) / (double)(1024 * 1024)
           << "; page_table num: " << std::get<1>(mem) / (double)(1024 * 1024);

    //ASSERT(false) << "temp debug exit";
#if 0
    Learner::train(global_table.get_table(0));
    LOG(4) << "server uses " << t.passed_sec() << " seconds for train";
    global_table.get_table(0).prepare_models();
    LOG(4) << "total keys: "
           << global_table.get_table(0).sc->index->sorted_array_size
           << "; B+tree depth: " << global_table.get_table(0).data.depth
           << "; Leaf size: " << sizeof(Leaf);
#endif

    //global_table.get_table(0).dump_table_models();

  } else {
    //Learner::retrain_static(global_table.get_table(0), FLAGS_model_config);
    //global_table.get_table(0).prepare_models();
  }

  // compute the XCache size
  auto model = ModelConfig::load(FLAGS_model_config);
  auto ml_sz = model.stage_configs[1].model_n * (8 + 4 + 4 + 2);
  auto pt_sz = BlockPager<Leaf>::allocated * sizeof(u64);

  LOG(4) << "XCache ml sz: " << ml_sz / (1024 * 1024.0)
         << "MB, page table sz: " << pt_sz / (1024 * 1024.0) << " MB"
         << "total KV sz: " << BlockPager<Leaf>::allocated * sizeof(Leaf) / (1024 * 1024 * 1024.0) << " GB";

  auto& t = global_table.get_table(0).data;
  for (uint d = 1; d <= t.depth; ++d)
    LOG(4) << "index sz level #" << d << ": "
           << t.calculate_index_sz(d) / (1024 * 1024.0) << " MB";

#endif

  //test_lidx.finish_insert();
  //test_lidx.finish_train();


}

} // namespace server

} // namespace fstore

using namespace fstore::server;

int
main(int argc, char** argv)
{
  //rocksdb::port::InstallStackTraceHandler();

  gflags::ParseCommandLineFlags(&argc, &argv, true);



  /**
   * First we load the configuration file.
   */
  auto config = global_config();
  LOG(2) << "use configuration: " << config.to_str()
         << " of config file: " << FLAGS_config_file;

  global_mem_sz = config.total_mem_sz();

  /**
   * Then we initializer the server memory layout
   */
  auto& rm = global_memory_region();
  region_desc_vec_t rm_desc = {
    { "meta",
      2 * MB }, // the first 2MB of memory is reserved for internal usage
    { "page", config.page_mem_sz + 1024 }
  };
  LOG(4) << "use page mem: " << config.page_mem_sz;
  StartUp::register_regions(rm, rm_desc);
  rm.make_rdma_heap();

  rdma::ConnectHandlers::register_cc_handler(global_rdma_ctrl(), global_nics());

  LOG(4) << "Memory layout of the server: " << rm.to_str();
#if 1
  /**
   * Initialize the memory area for leaf node.
   */
  auto page_desc = rm.get_region("page");
  BlockPager<Leaf>::init((char*)(page_desc.addr), page_desc.size);

  /*
    The page_id with 0 is reserved for an invalid page.
    We manually allocate the first specific page, so that the page id should
    never be zero.
  */
  auto page = BlockPager<Leaf>::allocate_one();
  ASSERT(BlockPager<Leaf>::page_id(page) == 0);

  // write a dummy value to the meta region
  char* meta_buf = rm.get_region("meta").get_as_virtual();
  *((u64*)meta_buf) = 0xdeadbeaf;

  PBarrier bar(FLAGS_threads + 1);
  auto handlers = Workers::bootstrap_all(FLAGS_threads, FLAGS_id, bar);

  for (auto h : handlers) {
    h->start();
    usleep(100);
  }
  r2::compile_fence();
  LOG(4) << "server wait for threads to join ...";

  LOG(4) << "Start populating DB: " << FLAGS_db_type << " with num: " << FLAGS_ycsb_num;
  load_db();
  LOG(4) << "Load DB done.";
  r2::compile_fence();
  LOG(4) << "bar wait";
  bar.wait();
#endif

  global_rdma_ctrl().bind(FLAGS_port);


#if 1
  auto pt = Thread<double>([]() {
    PageMonitor<BlockPager<Leaf>> alloc_monitor;

    std::vector<u64> prev_insert_counts (FLAGS_threads,0);
    std::vector<u64> prev_invalid_counts(FLAGS_threads, 0);
    //for (uint i = 0; i < 120; ++i) {

    r2::Timer t;

    while (1) {
      t.reset();
      sleep(2);

      // count all inserts
      u64 insert_sum  = 0;
      u64 invalid_sum = 0;

      for(uint i = 0;i < all_inserts.size();++i) {
        auto temp =  *(all_inserts[i]);
        insert_sum += temp - prev_insert_counts[i];
        prev_insert_counts[i] = temp;

        temp = *(all_invalids[i]);
        invalid_sum += temp - prev_invalid_counts[i];
        prev_invalid_counts[i] = temp;
      }
      auto passed_msec = t.passed_msec();
      double insert_rate = insert_sum / passed_msec * 1000000;
      double invalid_rate = invalid_sum / passed_msec * 1000000;
      LOG(0) << "insert rate :"  << insert_rate << " " << invalid_rate;
#if 1
      LOG(2) << "server has [" << alloc_monitor.alloced_thpt()
             << "] pages/sec; total alloced: " << BlockPager<Leaf>::allocated;

      auto index_sz = global_table.get_table(0).data.node_sz;
      auto KV_sz = index_sz + BlockPager<Leaf>::allocated * sizeof(Leaf);
      auto estimated_kvs =
        BlockPager<Leaf>::allocated * (IM / 2 + 1) / (1000 * 1000);
      LOG(4) << "B+Tree index sz: " << index_sz / (1024 * 1024.0) << " MB; "
             << "Tree depth: " << global_table.get_table(0).data.depth
             << " and KV sz:" << KV_sz / (1024 * 1024 * 1024.0) << " GB"
             << "; estimated KV pairs: " << estimated_kvs << " M";
#else
#endif
    }
    return 0.0;
  });
  pt.start();
#endif

#if 0
  Timer t;
  usize epoches = 0;
  auto prev_pages = BlockPager<Leaf>::allocated;
  while (t.passed_sec() < 360) {
    r2::compile_fence();
    sleep(1);
    // retrain
    //if (0) {

    auto temp = BlockPager<Leaf>::allocated;
    //if (temp != prev_pages)
    if (1)
    {
      Timer tt;
      bool verbose = true;
      const auto gap = Serializer::sizeof_submodel<LRModel<>>();
      auto res = global_table.get_table(0).xcache->force_train_submodels(
        global_table.get_table(0).data,
        global_table.get_table(0).submodel_buf,
        //nullptr,
        gap,
        verbose);

      //      XLearner::serialize_x(global_table.get_table(0));

      LOG(4) << "Retrain " << res << " submodels using "
             << tt.passed_msec() / 1000000 << " sec.";
      prev_pages = temp;
    }
  }
#else
  // update in background
  auto model_0 = new Thread<double>([]() {
    auto& tab = global_table.get_table(0);
    r2::Timer t;
    int count = 0;

    retry:
    auto x = tab.xcache;
    r2::compile_fence();
    while(tab.submodel_buf == nullptr) {
      r2::compile_fence();
    }
    while (1) {
#if 1
    if (t.passed_sec() >= 1) {
      t.reset();
      LOG(4) << count << " models trained in the past 2 seconds";
      count = 0;
    }
#endif

      for (auto c : all_channels) {
        usize counter = 5;
        while (!c->isEmpty()) {
          auto ret = c->dequeue();
          if (ret) {
            const auto gap = Serializer::sizeof_submodel<LRModel<>>() +
                             sizeof(u64) + sizeof(u64);

            // retrain the specific model
            auto temp_x = tab.xcache;
            r2::compile_fence();
            if (temp_x != x) {
              //sleep(100);
              goto retry;
            }

            if (ret.value() >= temp_x->sub_models.size() / 2) {
              //continue;
            }
            if (ret.value() > temp_x->sub_models.size()) {
              //continue;
            }
            auto& m = temp_x->sub_models[ret.value()];
            m.train(tab.data, FLAGS_step);
            count += 1;
            //Serializer::serialize_submodel(
            //              m, tab.submodel_buf + static_cast<u64>(ret.value()) * gap);
          }
        }
      }
    }
    return 0;
  });
  model_0->start();

  auto model_1 = new Thread<double>([]() {
    auto& tab = global_table.get_table(0);
    r2::Timer t;
    int count = 0;

    retry:
    auto x = tab.xcache;
    r2::compile_fence();
    while (tab.submodel_buf == nullptr) {
      r2::compile_fence();
    }
    while (1) {
#if 1
    if (t.passed_sec() >= 1) {
      t.reset();
      LOG(4) << count << " models trained in the past 2 seconds in model #1";
      count = 0;
    }
#endif
      for (auto c : all_channels_1) {
        while (!c->isEmpty()) {
        auto ret = c->dequeue();


        if (ret) {
          const auto gap = Serializer::sizeof_submodel<LRModel<>>() +
                           sizeof(u64) + sizeof(u64);

          // xcache has been updated
          auto temp_x = tab.xcache;
          r2::compile_fence();
          if (temp_x != x) {
            //sleep(100);
            goto retry;
          }

          // retrain the specific model
          auto& m = temp_x->sub_models[ret.value()];
          if (ret.value() < temp_x->sub_models.size() / 2) {
            continue;
          }

          if (ret.value() > temp_x->sub_models.size()) {
            //ASSERT(false);
            continue;
          }
          m.train(tab.data, FLAGS_step);
          count += 1;
          //Serializer::serialize_submodel(
          //            m, tab.submodel_buf + static_cast<u64>(ret.value()) * gap);
        }
        }
      }
    }
    return 0;
  });
  model_1->start();
#endif

#if 1
  auto model_2 = new Thread<double>([]() {
    auto& tab = global_table.get_table(0);
    r2::Timer t;
    int count = 0;

    while (1) {
#if 0
      if (t.passed_sec() >= 1) {
        t.reset();
        LOG(4) << count << " models trained in the past 2 seconds in model #2";
        count = 0;
      }
#endif
      for (auto c : all_channels_2) {
        while (!c->isEmpty()) {
          auto ret = c->dequeue();

          if (ret) {
            const auto gap = Serializer::sizeof_submodel<LRModel<>>() +
                             sizeof(u64) + sizeof(u64);

            // retrain the specific model
            auto& m = tab.xcache->sub_models[ret.value()];
            m.train(tab.data, FLAGS_step);
            count += 1;
            // Serializer::serialize_submodel(
            //              m, tab.submodel_buf + static_cast<u64>(ret.value()) * gap);
          }
        }
      }
    }
    return 0;
  });
  //model_2->start();
#endif

  while (1) {
    sleep(1);
    r2::compile_fence();
    if (in_retrain) {
      // start to retrain
      auto loaded_model = ModelConfig::load(FLAGS_model_config);
      auto expected_model_num =
        loaded_model.stage_configs[1].model_n* 2;
      auto x = new X(expected_model_num);
      LOG(4) << "retrain increase model num to : "
             << expected_model_num;

      auto& tab = global_table.get_table(0);

      BStream it(tab.data, 0);
      bool whether_dump = false;
      // bool  whether_dump = true;
      r2::Timer t;

      auto oldx = tab.xcache;
      x->train_dispatcher(it, whether_dump);


#if 1
      auto trained =
        x->train_submodels(tab.data, nullptr, 0, FLAGS_step, true, false);

#if 1
      LOG(4) << Aug::aug_zero(x->sub_models) << " models augmented";

      x->train_submodels(tab.data, nullptr, 0, FLAGS_step);
#endif
#endif


      r2::compile_fence();
      auto allocator = r2::AllocatorMaster<0>::get_thread_allocator();
      allocator->dealloc(tab.submodel_buf);
      tab.submodel_buf = nullptr;
      r2::compile_fence();
      tab.xcache = x;

      auto passed_msec = t.passed_msec();

      r2::compile_fence();
      auto mem = XLearner::serialize_x(tab);
      LOG(4) << "serialize done  using " << passed_msec << " msec";
    }
    in_retrain = false;
  }

  for (auto h : handlers)
    h->join();
  //pt.join();

  // shall never return
  LOG(4) << "all server threads done, exit";

  return 0;
}
