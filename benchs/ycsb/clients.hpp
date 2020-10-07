#pragma once

#include "mem_region.hpp"

#include "r2/src/futures/rdma_future.hpp"
#include "r2/src/random.hpp"
#include "r2/src/rdma/connect_manager.hpp"

#include "utils/cc_util.hpp"

//#include "../../cli/lib.hpp"
#include "../../server/proto.hpp"

#include "../../src/data_sources/txt_workloads.hpp"

#include "../micro/clients/lib.hpp"
//#include "../micro/net_config.hpp"
#include "../arc/val/net_config.hh"

#include "../micro/latency_calculator.hpp"

#include "ycsba.hpp"

#include "../../xcli/mod.hh"
#include "../../xcli/cached_mod.hh"
//#include "../../xcli/cached_mod_v2.hh"
#include "../../xcli/cached_mod_v3.hh"

#include "../server/loaders/ycsb.hpp"

namespace fstore {

DEFINE_int32(cache_sz_m, 1, "usd cache sz in MB");

using namespace server;
using namespace platforms;
using namespace r2;
using namespace r2::util;

Tree sample_cache;

__thread u64 data_transfered;
__thread u64 err_num;

#define OSM 0
std::vector<u64> osm_keys;

namespace bench {

RdmaCtrl&
global_rdma_ctrl();
RegionManager&
global_memory_region();

extern volatile bool running;
extern SC* sc;

using Worker = Thread<double>;

std::shared_ptr<XDirectTopLayer> xcache = nullptr;
XCachedClientV2* xglobal = nullptr;

class Client
{
public:
  static std::vector<Worker*> bootstrap(std::vector<Statics>& statics,
                                        const Addr& server_addr,
                                        const std::string& server_host,
                                        const u64& server_port,
                                        PBarrier& bar,
                                        u64 num_threads,
                                        u64 my_id,
                                        u64 concurrency = 1)
  {
    std::vector<Worker*> clis;

#if OSM
    {
      u64 loaded = 0;
      // load OSM data
      std::ifstream ifs("osm_uni_100m.txt");
      std::string line;
      while (std::getline(ifs, line)) {

        // convert to u64
        u64 key;
        std::istringstream iss(line);
        iss >> key;
        // keys.push_back(key);
        osm_keys.push_back(key);

        loaded += 1;
        if (loaded >= 100000000) {
          break;
        }
      }
      //LOG(4) << "osm_keys: "<< osm_keys.size(); ASSERT(false);
    }
#endif
    for (uint thread_id = 0; thread_id < num_threads; ++thread_id) {
      clis.push_back(
        /**
          Main test threads body.
          The whole function just make many bootstrap processes,
          including fetch the remote model, and setup RPC/RDMA connections.
          After bootstrap done, it creates many coroutines, which executes
          benchmark code. i.e. for(uint i = 0;i < concurrency;++i) {
               r.spawnr([]() {
                  // ... real test functions
               });
           }
         */
        new Worker([&,
                    thread_id,
                    my_id,
                    server_addr,
                    concurrency,
                    server_port,
                    server_host]() {
          CoreBinder::bind(thread_id);
          // we use the first thread as bootstrap
          Addr bootstrap_addr({ .mac_id = server_addr.mac_id, .thread_id = 0 });

          // first we fetch the server meta
          auto all_devices = RNicInfo::query_dev_names();
          ASSERT(!all_devices.empty()) << "RDMA must be supported.";

          auto nic_id = VALNic::choose_nic(thread_id);
          ASSERT(nic_id < all_devices.size()) << "wrong dev id:" << nic_id;

          RNic nic(all_devices[VALNic::choose_nic(thread_id)]);

          // First we register the memory
          auto ret = global_rdma_ctrl().mr_factory.register_mr(
            thread_id,
            global_memory_region().base_mem_,
            global_memory_region().base_size_,
            nic);
          ASSERT(ret == SUCC) << "failed to register memory region.";

          usize local_buf_sz = 4096 * 4096 * 64;
          char* local_buf = new char[local_buf_sz];
          assert(local_buf != nullptr);

          ret = global_rdma_ctrl().mr_factory.register_mr(
            thread_id + 1024,
            local_buf,
            local_buf_sz,
            // global_memory_region().base_mem_,
            // global_memory_region().base_size_,
            nic);
          ASSERT(ret == SUCC) << "failed to register memory region " << ret
                              << " with id: " << thread_id + 73;

          auto adapter =
            Helper::create_thread_ud_adapter(global_rdma_ctrl().mr_factory,
                                             global_rdma_ctrl().qp_factory,
                                             nic,
                                             my_id,
                                             thread_id);

          while (adapter->connect(bootstrap_addr,
                                  ::rdmaio::make_id(server_host, server_port),
                                  0) != SUCC) {
            sleep(1);
          }

          // LOG(4) << "[YCSB] client #" << thread_id << " bootstrap connect
          // done to server";

          RScheduler r;
          RPC rpc(adapter);
          rpc.spawn_recv(r);

          r.spawnr([&](handler_t& h, RScheduler& r) {
            auto id = r.cur_id();

            auto ret = rpc.start_handshake(bootstrap_addr, r, h);
            ASSERT(ret == SUCC) << "start handshake error: " << ret;

            // fetch the server meta
            auto server_meta =
              Helper::fetch_server_meta(bootstrap_addr, rpc, r, h);
            auto server_addr = bootstrap_addr;

            // some sanity checks
            ASSERT(server_meta.num_threads > 0 && server_meta.num_threads < 36)
              << "fetch server  thread:" << server_meta.num_threads;
            //LOG(4) << "fetch server meta num threads: "
            //     << server_meta.num_threads;
            if (server_meta.num_threads > 0) {
              // there are more threads at server, so we re-dispatch the
              // client's connections
              uint connect_id = thread_id % server_meta.num_threads;
              server_addr =
                Addr({ .mac_id = server_addr.mac_id, .thread_id = connect_id });
              // LOG(4) << "client #" << thread_id << " re-conenct to : " <<
              // connect_id;

              if (connect_id != 0) {
                // re-start the handshake to the server
                rpc.end_handshake(bootstrap_addr);

                while (
                  adapter->connect(server_addr,
                                   ::rdmaio::make_id(server_host, server_port),
                                   connect_id) != SUCC) {
                  sleep(1);
                }

                auto ret = rpc.start_handshake(server_addr, r, h);
                ASSERT(ret == SUCC) << "start handshake error: " << ret;
              } // end re-connect to appropriate thread
            }
            // LOG(4) << "reconnect done #" << thread_id;

            /**
             * Fetch server's MR
             */
            RemoteMemory::Attr remote_mr;
            while (RMemoryFactory::fetch_remote_mr(
                     server_addr.thread_id,
                     ::rdmaio::make_id(server_host, server_port),
                     remote_mr) != SUCC) {
              sleep(1);
            }

            u64 qp_id = my_id << 32 | (thread_id + 1);

          // then we create the specificed QP
#if 1
            auto qp = Helper::create_connect_qp(global_rdma_ctrl().mr_factory,
                                                global_rdma_ctrl().qp_factory,
                                                nic,
                                                qp_id,
                                                thread_id,
                                                remote_mr,
                                                server_addr,
                                                rpc,
                                                r,
                                                h);
#else
            // now we use the connect manager for us to create QP
            RemoteMemory::Attr remote_mr;
            while (RMemoryFactory::fetch_remote_mr(
                     remote_connect_id,
                     ::rdmaio::make_id(server_host, server_port),
                     remote_mr) != SUCC) {
            }

          RemoteMemory::Attr local_mr;

          auto qp = Helper::create_connect_qp(global_rdma_ctrl().mr_factory,
                                              global_rdma_ctrl().qp_factory,
                                              nic,
                                              qp_id,
                                              thread_id,
                                              remote_mr,
                                              addr,
                                              rpc,
                                              r,
                                              h);
#endif
            ASSERT(qp != nullptr);
            // LOG(4) << "create connected qp done #" << thread_id;

#define CACHED_SC 0
#if !CACHED_SC  // use a global shared SC
            XCachedClient *x = nullptr;

            if (thread_id == 0 && FLAGS_eval_type == "sc") {
              //sc = ModelFetcher::bootstrap_remote_sc(
              //0, rpc, server_addr, qp, r, h);

              auto ret = XBoot::bootstrap_xcache(0, rpc,server_addr,qp, h,r);
              auto xboot = std::get<0>(ret);
              xcache = std::get<1>(ret);

              r2::Timer t;
#if 1  // per-model update
              char* send_buf =
                (char*)AllocatorMaster<>::get_thread_allocator()->alloc(40960);

              for(uint i = 0;i < xcache->submodels.size();++i) {
                //xcache->submodels[i] = xboot.update_submodel(i,send_buf,h,r);
                xboot.update_submodel(xcache->submodels[i],i,send_buf,h,r);
              }

              AllocatorMaster<>::get_thread_allocator()->dealloc(send_buf);

#else
              // batch update all
              //char* send_buf =
              //                (char*)AllocatorMaster<>::get_thread_allocator()->alloc(1024 * xcache->submodels.size());
              char* send_buf =
                (char*)AllocatorMaster<>::get_thread_allocator()->alloc(xboot.max_read_length + 1024);

              ASSERT(send_buf != nullptr);

              xboot.batch_update_all(xcache->submodels,true,send_buf, h,r);
              AllocatorMaster<>::get_thread_allocator()->dealloc(send_buf);

#endif
              LOG(4) << "update second layer done using : " << t.passed_msec() << " msec";
            }

#else
            auto submodel_sz = Serializer::sizeof_submodel<LRModel<>>() + sizeof(u64) + sizeof(u64);

            usize cached_entries =
              (FLAGS_cache_sz_m * 1024 * 1024) / submodel_sz;
            //usize cached_entries = FLAGS_cache_sz_m; // there would be fragmentation, so the actuall sz of the XCache would be smaller
            // so we manally passe ot
            //ASSERT(cached_entries > 0) << " cached entries must be larger than 0";

            //LOG(4) << "cached entries: "<< cached_entries << "Inner sz: " << sizeof(Inner) << "; sz m : "<< FLAGS_cache_sz_m << " per model sz: " << submodel_sz;
            auto page_start = server_meta.page_addr;
            auto x =
              XCachedClient::create(0, cached_entries, rpc, server_addr, page_start, h, r);
#if 1
            auto& factory = rpc.get_buf_factory();
            auto send_buf = factory.alloc(4096);
            x->qp = qp;
#endif

            //cached_entries = cached_entries * 1024 * 1024;
            //if (thread_id == 0 && FLAGS_eval_type == "sc") {
            auto x2 = XCachedClientV2::create(0,
                                         FLAGS_cache_sz_m * 1024 * 1024,
                                         rpc,
                                         server_addr,
                                         page_start,
                                         h,
                                         r);
            x2->qp = qp;
            if (thread_id == 0) {
              x2->fill_all_submodels(send_buf, h, r);
              r2::compile_fence();
              xglobal = x2;
            }

              //}
#endif

            if (thread_id == 0 && FLAGS_eval_type == "nt" && false) {
              // B+Tree evaluations are moved to a separate branch
              // sanity check remote B+Tree
              auto& factory = rpc.get_buf_factory();
              auto send_buf = factory.alloc(512);

              Marshal<TableModel>::serialize_to({ .id = 0 }, send_buf);
              char reply_buf[64];
              auto ret =
                rpc.call({ .cor_id = r.cur_id(), .dest = server_addr },
                         TREE_META,
                         { .send_buf = send_buf,
                           .len = sizeof(TableModel),
                           .reply_buf = reply_buf,
                           .reply_cnt = 1 });
              r.pause_and_yield(h);
              // sanity check the root

              TreeRoot *tree_meta = (TreeRoot *)reply_buf;
              LOG(4) << "tree meta: " << tree_meta->addr << " " << tree_meta->base;

              // read
              qp->send(
                { .op = IBV_WR_RDMA_READ,
                  .flags = IBV_SEND_SIGNALED,
                  .len = sizeof(Inner),
                  //.len = 64,
                  .wr_id = r.cur_id() },
                { .local_buf = send_buf, .remote_addr = tree_meta->addr - tree_meta->base, .imm_data = 0 });
              RdmaFuture::spawn_future(r, qp, 1);
              r.pause(h);

              Inner* inner = (Inner*)send_buf;
              LOG(4) << "client fetch root num key: " << inner->num_keys;

              ASSERT(false);
            }
            bar.wait();
            //ASSERT(xglobal->cached_models.size() == x2->cached_models.size());
#if CACHED_SC
            for (uint i = 0;i < xglobal->cached_models.size();++i) {
              x2->cached_models[i] = xglobal->cached_models[i];
              x2->ext_size[i] = xglobal->ext_size[i];
              x2->ext_addrs[i] = xglobal->ext_addrs[i];
            }
            x2->cur_cached_tt = xglobal->cur_cached_tt;
#endif

            //ASSERT(x2->cached_models.size() != 0);

            if (FLAGS_eval_type == "sc") {
#if !CACHED_SC
              ASSERT(xcache != nullptr);
#else
              x->qp = qp;
#endif
            }

            u64 thread_random_seed = FLAGS_seed + 73 * thread_id + FLAGS_id * 8;
            std::vector<WorkloadDesc> workloads;

#if OSM
            ::fstore::sources::TXTWorkload ycsb(&osm_keys,
                                                thread_random_seed +
                                                id * 0xddd + 0xaaa);
            using W = ::fstore::sources::TXTWorkload;
#else
            //YCSBCWorkload
            YCSBCWorkloadUniform
              ycsb(FLAGS_total_accts,
                   thread_random_seed +
                   id * 0xddd + 0xaaa,
                   FLAGS_need_hash);


            using W = YCSBCWorkloadUniform;
            //using W = YCSBCWorkload;
#endif

#if 1
              if (thread_id == 0 && sc != nullptr)
            {
              // sanity check predicting performance
              utils::DistReport<usize> report;
              utils::DistReport<usize> report_1;

              YCSBHashGenereator it(0, FLAGS_total_accts);
              CDF<u64> page_span_cdf("xx");
              u64 max_page_span = 0;
              u64 min_page_span = 102400L;

              u64 max_span = 0;
              u64 min_span = std::numeric_limits<u64>::max();

              for (it.begin(); it.valid(); it.next()) {
                auto predict = sc->get_predict(it.key());
                auto page_span = sc->mp->decode_mega_to_entry(predict.end) -
                                 sc->mp->decode_mega_to_entry(predict.start) + 1;
                min_page_span =
                  std::min(static_cast<u64>(page_span), min_page_span);
                max_page_span =
                  std::max(static_cast<u64>(page_span), max_page_span);
                page_span_cdf.insert(page_span);
                report.add(page_span);

                auto span = predict.end - predict.start + 1;
                min_span = std::min(static_cast<u64>(span),min_span);
                max_span = std::max(static_cast<u64>(span), max_span);
                report_1.add(span);
              }

              LOG(4) << "exam results, min: " << report.min
                     << " ; max: " << report.max << " avg: " << report.average;

              LOG(4) << "all predicts verify done"
                     << " , max page span: " << max_page_span << " over "
                     << FLAGS_total_accts;

              LOG(4) << "predict range average: " << report_1.average << " ;min: " << min_span << " " << "max:" << max_span;
              page_span_cdf.finalize();
              FILE_WRITE("span.py", std::ofstream::out)
                << page_span_cdf.dump_as_np_data();
            }
#endif
#if 1
            auto search_depth = sample_cache.depth;
            FClient *fc = nullptr;
            NTree* nt = nullptr;

            if (FLAGS_workloads == "ycsba")
            {
              using YCSBAA = YCSBA<50, 50,W>;
              if (FLAGS_eval_type == "rpc")
                workloads =
                  YCSBAA::get_rpc_workloads(server_addr, ycsb, r, rpc);
              else if (FLAGS_eval_type == "sc") {
                //ASSERT(false);
                //fc = new FClient(
                //                  qp, sc, server_meta.page_addr, server_meta.page_area_sz);
                //workloads =
                //                  YCSBAA::sc_workloads(server_addr, ycsb, fc, r, rpc, nullptr);
#if !CACHED_SC
                auto xc =
                  new XCacheClient(xcache, &rpc, qp, server_meta.page_addr);
                workloads =
                  YCSBAA::sc_workloads(server_addr, ycsb, xc, r, rpc, nullptr);
#else
                ASSERT(false);
                workloads =
                  YCSBAA::x_workloads(server_addr, ycsb, x, r, rpc, nullptr);

#endif

              } else if (FLAGS_eval_type == "nt") {
                //LOG(3) << "init nt using qp: " << qp;
                //auto search_depth = FLAGS_tree_depth - std::min(FLAGS_tree_depth,static_cast<i32>(sample_cache.depth));
                nt = new NTree(qp,
                                      server_meta.page_addr,
                                      server_meta.page_area_sz,
                                      search_depth,
                                      thread_random_seed + 0xdeafbeaf + 0xddaa,true);
                workloads = YCSBAA::nt_workloads(server_addr, ycsb, nt, r, rpc);
              } else {
                ASSERT("un-supported evaluation type");
              }
            }
            else if (FLAGS_workloads == "ycsbb")
            {
              using YCSBAB = YCSBA<90, 10,W>;
              auto search_depth =
                FLAGS_tree_depth -
                std::min(FLAGS_tree_depth,
                         static_cast<i32>(sample_cache.depth));
              if (FLAGS_eval_type == "rpc")
                workloads =
                  YCSBAB::get_rpc_workloads(server_addr, ycsb, r, rpc);
              else if (FLAGS_eval_type == "sc") {

#if !CACHED_SC
                auto xc =
                  new XCacheClient(xcache, &rpc, qp, server_meta.page_addr);
                workloads =
                  YCSBAB::sc_workloads(server_addr, ycsb, xc, r, rpc, nullptr);
#else
                workloads =
                  YCSBAB::x_workloads(server_addr, ycsb, x, r, rpc, nullptr);

#endif

                //ASSERT(false);
                //YCSBAB::sc_workloads(server_addr, ycsb, fc, r, rpc, nullptr);
              } else if (FLAGS_eval_type == "nt") {
#if 0
                auto search_depth =
                  FLAGS_tree_depth -
                  std::min(FLAGS_tree_depth,
                           static_cast<i32>(sample_cache.depth));
#endif
                auto search_depth = sample_cache.depth;
                nt = new NTree(qp,
                                      server_meta.page_addr,
                                      server_meta.page_area_sz,
                                      search_depth,
                                      thread_random_seed + 0xdeafbeaf + 0xddaa,thread_id == 0);
                workloads = YCSBAB::nt_workloads(server_addr, ycsb, nt, r, rpc);
              } else {
                ASSERT("un-supported evaluation type");
              }
            }
            else if (FLAGS_workloads == "ycsbc")
            {

              auto search_depth = sample_cache.depth;

              using YCSBAB = YCSBA<100, 0,W>;
              if (FLAGS_eval_type == "rpc")
                workloads =
                  YCSBAB::get_rpc_workloads(server_addr, ycsb, r, rpc);
              else if (FLAGS_eval_type == "sc") {
                //fc = new FClient(
                //                  qp, sc, server_meta.page_addr, server_meta.page_area_sz);
#if !CACHED_SC
                auto xc = new XCacheClient(xcache, &rpc, qp, server_meta.page_addr);
                workloads =
                  YCSBAB::sc_workloads(server_addr, ycsb, xc, r, rpc, nullptr);
#else
                //workloads =
                //YCSBAB::x_workloads(server_addr, ycsb, x, r, rpc, nullptr);
                //ASSERT(x2->cur_cached_tt != 0);
                workloads = YCSBAB::x2_workloads(server_addr,ycsb,x2,r,rpc,nullptr);


#endif
              } else if (FLAGS_eval_type == "nt") {

                nt = new NTree(qp,
                                      server_meta.page_addr,
                                      server_meta.page_area_sz,
                                      search_depth,
                                      //FLAGS_tree_depth,
                                      thread_random_seed + 0xdeafbeaf + 0xddaa, thread_id == 0);
#if 0
                workloads = YCSBAB::nt_hybrid_workloads(server_addr,
                                                        ycsb,
                                                        nt,
                                                        r,
                                                        rpc,
                                                        thread_random_seed +
                                                          0xdeafbeaf + 0xddaa);
#else

                workloads = YCSBAB::nt_workloads(server_addr, ycsb, nt, r, rpc);
#endif
              } else if (FLAGS_eval_type == "hybrid") {
                fc = new FClient(
                  qp, sc, server_meta.page_addr, server_meta.page_area_sz);

#if 0
                if (FLAGS_id % 2 == 0 ) {
                  workloads =
                    YCSBAB::get_rpc_workloads(server_addr, ycsb, r, rpc);
                } else {
                  //workloads = YCSBAB::sc_workloads(
                  //                    server_addr, ycsb, fc, r, rpc, nullptr);
                }
#else
                workloads = YCSBAB::hybrid_workloads(server_addr,
                                                     ycsb,
                                                     fc,
                                                     r,
                                                     rpc,
                                                     nullptr,
                                                     thread_random_seed +
                                                       0xdeafbeaf + 0xddaa);
#endif
              } else {
                ASSERT(false) << "un-supported evaluation type";
              }
            }

            ASSERT(workloads.size() > 0);
#endif
            if (thread_id == 0) {
              LOG(3) << "eval using [" << FLAGS_workloads
                     << "]; function type:" << FLAGS_eval_type;
              LOG(3) << "sanity check value sz: " << sizeof(ValType);
            }

            Workload executor(thread_id + 0xdeadbeaf * r.cur_id());

            // finally, we start coroutine for test functions
            for (uint i = 0; i < concurrency; ++i) {
              r.spawnr([&, i, thread_id, server_meta](R2_ASYNC) {

                // reset adapter's lkey
                RemoteMemory::Attr local_mr;
                ASSERT(global_rdma_ctrl().mr_factory.fetch_local_mr(
                         thread_id + 1024, local_mr) == SUCC);
                adapter->reset_key(local_mr.key);

                //char* send_buf =
                  //(char*)AllocatorMaster<>::get_thread_allocator()->alloc(
                  // std::max(static_cast<u64>(4096), FLAGS_rdma_payload));
                //                  local_buf + 4096 * 1024 * i;

                Timer t;
                FlatLatRecorder lats;
                FlatLatRecorder get_lats;
                FlatLatRecorder put_lats;

#if 0
                char* send_buf =
                  (char*)AllocatorMaster<>::get_thread_allocator()->alloc(
                    16 * sizeof(Leaf));
#else
                char *send_buf = local_buf + 4096 * 1024 * i;
#endif

                ASSERT(4096 * 1024 >= 16 * sizeof(Leaf));

                qp->local_mem_ = local_mr;

                u64 execute_count = 0;
                DistReport<double> index_lat;
                DistReport<double> value_lat;
                DistReport<double> model_lat;
#if 0
                YCSBCWorkload
                  // YCSBCWorkloadUniform
                  ww(FLAGS_total_accts,
                    thread_random_seed + i * 0xddd + 0xaaa,
                    FLAGS_need_hash);
#endif

#if OSM
                ::fstore::sources::TXTWorkload ww(
                  &osm_keys, thread_random_seed + id * 0xddd + 0xaaa);
#endif

                while (running) {
                  execute_count += 1;
#if 1

#if OSM
                  u64 key = ww.next_key();
#else
                  u64 key = 0;
#endif
                  data_transfered = 0;
                  t.reset();
                  auto idx = executor.run(workloads, yield, send_buf, key).first;
                  //if (fc != nullptr)
                  //                    statics[thread_id].increment_gap_1(fc->cur_data_transfered);
                  statics[thread_id].increment_gap_1(data_transfered);
                  statics[thread_id].increment();
                  if (thread_id == 0 && R2_COR_ID() == 2) {
                    auto lat = t.passed_msec();
                    lats.add_one(lat);
                    if (idx == 0)
                      get_lats.add_one(lat);
                    else {
                      put_lats.add_one(lat);
                    }

                    //t.reset();
                    statics[thread_id].data.lat = lats.get_lat();
                  }
#else
                  u64 remote_addr =
                    server_meta.page_addr +
                    executor.rand.rand_number<u64>(0,
                                          server_meta.page_area_sz -
                                            (1024));

                  auto res = qp->send({ .op = IBV_WR_RDMA_READ,
                                        .flags = IBV_SEND_SIGNALED,
                                        .len = Leaf::value_offset(0),
                                        .wr_id = r.cur_id() },
                                      { .local_buf = send_buf,
                                        .remote_addr = remote_addr,
                                        .imm_data = 0 });
                  RdmaFuture::spawn_future(r, qp, 1);
                  r.pause(yield);
                  statics[thread_id].increment();

#endif
                  R2_YIELD;

#if MODEL_COUNT
                  if (thread_id == 0 && R2_COR_ID() == 2 && FLAGS_id == 1) {
                    // report
                    if (execute_count > 5000000) {
                      execute_count = 0;

                      auto report = xcache->report_model_selection();
                      LOG(4) << "report avg: " << report.others.average << " "
                             << "; min: " << report.others.min << "; max:" << report.others.max
                             << " " << report.dump_as_np_data();
                      ASSERT(false);
                    }
                  }
#endif
                  execute_count += 1;

#if COUNT_LAT && CACHED_SC
                  if (nt) {
                    index_lat.add((double)nt->rdma_index / nt->total);
                    value_lat.add((double)nt->rdma_value / nt->total);
                  }

                  if (x) {
                    index_lat.add((double)x->rdma_index / x->total_time);
                    value_lat.add((double)x->rdma_value / x->total_time);
                    model_lat.add((double)x->model_time / x->total_time);
                  }
#endif

#if 0
                  if (execute_count >= 10000000) {
                    if (nt) {
                      LOG(4) << "nt index lat: " << index_lat.average << "; nt value lat: " << value_lat.average;
                    }
                    if (x) {
                      LOG(4) << "x index lat: " << index_lat.average
                             << "; x value lat: " << value_lat.average << "; model: " << model_lat.average;
                    }
                    //ASSERT(false);
                    execute_count = 0;
                  }
#endif
                }
                if (thread_id == 0 && R2_COR_ID() == 2) {
                  LOG(4) << "get lats: " << get_lats.get_lat();
                  LOG(4) << "put lats: " << put_lats.get_lat();
                }


                R2_STOP();
                R2_RET;
              });
            }
            //ASSERT(sc != nullptr);
            routine_ret(h, r);
          });
          r.run();
          return 0.0;
        }));
    }
    return clis;
  }
}; // namespace bench

} // namespace bench

} // namespace fstore
