#pragma once

#include "core_workload.hpp"
#include "data_sources/ycsb/stream.hpp"
#include "data_sources/ycsb/workloads.hpp"

#include "../../cli/lib.hpp"
#include "../../xcli/mod.hh"

#include "../../xcli/cached_mod.hh"
#include "../../xcli/cached_mod_v2.hh"

#include "../../xcli/cached_mod_v3.hh"

namespace fstore {

extern __thread u64 data_transfered;

namespace bench {

using namespace r2;
using namespace r2::rpc;

using namespace sources::ycsb;


/*!
  A: the workload ratio of the first(get) workload, must be smaller than 1
  B: the workload ratio of another (put) workload
  \Note: in percentage
 */
template<int A, int B, typename W>
class YCSBA
{
public:
  static constexpr double ycsba_get_ratio = static_cast<double>(A) / 100;
  static constexpr double ycsba_put_ratio = static_cast<double>(B) / 100;

  static_assert(ycsba_get_ratio + ycsba_put_ratio == 1.0,
                "The workload ratio should sum up to one.");

  static void sanity_check_val(const KeyType& k, char* buf)
  {
    ValType* val = reinterpret_cast<ValType*>(buf);
    ASSERT(val->get_meta() == k)
      << "key: " << k << "; get meta:" << val->get_meta();
  }

  static u64 eval_sc(Workload& w,
                     const Addr& server_addr,
                     W& ycsb,
                     FClient* fc,
                     char* local_buf,
                     RPC& rpc,
                     RScheduler& r,
                     handler_t& h,
                     u64& get_counter,
                     u64& put_counter)
  {
    auto idx = w.rand.next() % 100;
    if (idx < 50) {
      // this is a get
      auto key = ycsb.next_key();

      // auto send_buf = (char *)AllocatorMaster<>::get_thread_allocator()
      //->alloc(10 * sizeof(Leaf));
      auto predict = fc->get_predict(key);
      auto res = fc->get_addr(key, predict, local_buf, r, h);

      auto code = std::get<0>(res); // success code
      auto v = std::get<1>(res);    // the actual value

      get_counter += 1;
      return v;
    } else {
      // this is a put
      char reply_buf[sizeof(ValType)];
      auto key = ycsb.next_key();

      auto& factory = rpc.get_buf_factory();
      // auto send_buf = factory.get_inline_buf();
      // auto send_buf = factory.alloc(sizeof(ValType) + sizeof(GetPayload));
      auto send_buf = local_buf + rpc.reserved_header_sz();

      using namespace ::fstore::server;
      GetPayload req = { .table_id = 0, .key = key };
      Marshal<GetPayload>::serialize_to(req, send_buf);

      auto ret = rpc.call({ .cor_id = r.cur_id(), .dest = server_addr },
                          PUT_ID,
                          { .send_buf = send_buf,
                            .len = sizeof(GetPayload) + sizeof(ValType),
                            .reply_buf = reply_buf,
                            .reply_cnt = 1 });

      r.pause_and_yield(h);
      put_counter += 1;
      return *((u64*)reply_buf);
    }
  }

  static std::vector<WorkloadDesc> get_rpc_workloads(const Addr& server_addr,
                                                     W& ycsb,
                                                     RScheduler& r,
                                                     RPC& rpc)
  {
    std::vector<WorkloadDesc> res;

    WorkloadDesc get_workload = {
      .name = "get",
      .frequency = ycsba_get_ratio,
      .fn = [&](handler_t& h, char* buf, u64 k) -> u64 {
        char reply_buf[sizeof(ValType)];
        auto key = ycsb.next_key();

        auto& factory = rpc.get_buf_factory();
        auto send_buf = factory.get_inline_buf();

        using namespace ::fstore::server;
        GetPayload req = { .table_id = 0, .key = key };
        Marshal<GetPayload>::serialize_to(req, send_buf);

        auto ret = rpc.call({ .cor_id = r.cur_id(), .dest = server_addr },
                            GET_ID,
                            //NULL_ID,
                            { .send_buf = send_buf,
                              .len = sizeof(GetPayload),
                              .reply_buf = reply_buf,
                              .reply_cnt = 1 });

        r.pause_and_yield(h);
        data_transfered += ValType::get_payload();

        // if get ratio is 100%, we sanity check the value
        if (ycsba_get_ratio == 1) {
          //sanity_check_val(key, reply_buf);
        }

        return *((u64*)reply_buf);
      },
      .executed = 0
    };

    WorkloadDesc put_workload = {
      .name = "put",
      .frequency = ycsba_put_ratio,
      .fn = [&](handler_t& h, char* buf, u64 k) -> u64 {
        char reply_buf[sizeof(ValType)];
        auto key = ycsb.next_key();

        auto& factory = rpc.get_buf_factory();
        // auto send_buf = factory.get_inline_buf();
        auto send_buf = factory.alloc(sizeof(ValType) + sizeof(GetPayload));

        using namespace ::fstore::server;
        GetPayload req = { .table_id = 0, .key = key };
        Marshal<GetPayload>::serialize_to(req, send_buf);


        if (1) {
          auto ret = rpc.call({ .cor_id = r.cur_id(), .dest = server_addr },
                              GET_ID,
                              { .send_buf = send_buf,
                                .len = sizeof(GetPayload),
                                .reply_buf = reply_buf,
                                .reply_cnt = 1 });

          r.pause_and_yield(h);
          //ASSERT(false);
        }

        auto ret = rpc.call({ .cor_id = r.cur_id(), .dest = server_addr },
                            PUT_ID,
                            { .send_buf = send_buf,
                              .len = sizeof(GetPayload) + sizeof(ValType),
                              .reply_buf = reply_buf,
                              .reply_cnt = 1 });

        r.pause_and_yield(h);
        factory.dealloc(send_buf);
        return *((u64*)reply_buf);
      },
      .executed = 0
    };

    res.push_back(get_workload);
    res.push_back(put_workload);
    return res;
  }

  static std::vector<WorkloadDesc> sc_workloads(const Addr& server_addr,
                                                W& ycsb,
                                                XCacheClient* x,
                                                RScheduler& r,
                                                RPC& rpc,
                                                char* buf)
  {
    std::vector<WorkloadDesc> res;

    WorkloadDesc get_workload = {
      .name = "get",
      .frequency = ycsba_get_ratio,
      .fn = [&server_addr, &ycsb, x, &r, &rpc](handler_t& h,
                                               char* local_buf, u64 k) -> u64 {
              auto key = ycsb.next_key();
              //auto key = k;

        auto v = x->get_direct(key, local_buf, h, r);
        if (v == ::fstore::SearchCode::None) {
          LOG(4) << "failed to search  key: "<< key;
          x->debug_get(key,local_buf,h,r);
          ASSERT(false);
        }
        if (unlikely(v == ::fstore::SearchCode::Unsafe)) {
          ASSERT(false);
          // fallback to RPC
          char reply_buf[sizeof(ValType)];

          auto& factory = rpc.get_buf_factory();
          // auto send_buf = factory.get_inline_buf();
          // auto send_buf = factory.alloc(sizeof(ValType) +
          // sizeof(GetPayload));
          auto send_buf = local_buf + rpc.reserved_header_sz();

          using namespace ::fstore::server;
          GetPayload req = { .table_id = 0, .key = key };
          Marshal<GetPayload>::serialize_to(req, send_buf);

          auto ret = rpc.call({ .cor_id = r.cur_id(), .dest = server_addr },
                              GET_ID,
                              { .send_buf = send_buf,
                                .len = sizeof(GetPayload),
                                .reply_buf = reply_buf,
                                .reply_cnt = 1 });

          r.pause_and_yield(h);
          // factory.dealloc(send_buf);
          return *((u64*)reply_buf);
        }
        //ASSERT(v == ::fstore::SearchCode::Ok) << "error code: "<< v << " for key: "<< key;
        //return v.value();
        return (u64)v;
      },
      .executed = 0
    };

    WorkloadDesc put_workload = {
      .name = "put",
      .frequency = ycsba_put_ratio,
      .fn = [&](handler_t& h, char* local_buf, u64 k) -> u64 {
        auto key = ycsb.next_key();
#if 1
        char reply_buf[sizeof(ValType)];

        auto& factory = rpc.get_buf_factory();
        // auto send_buf = factory.get_inline_buf();
        // auto send_buf = factory.alloc(sizeof(ValType) + sizeof(GetPayload));
        auto send_buf = local_buf + rpc.reserved_header_sz();

        using namespace ::fstore::server;
        GetPayload req = { .table_id = 0, .key = key };
        Marshal<GetPayload>::serialize_to(req, send_buf);

        auto ret = rpc.call({ .cor_id = r.cur_id(), .dest = server_addr },
                            PUT_ID,
                            { .send_buf = send_buf,
                              .len = sizeof(GetPayload) + sizeof(ValType),
                              .reply_buf = reply_buf,
                              .reply_cnt = 1 });

        r.pause_and_yield(h);
        // factory.dealloc(send_buf);
        return *((u64*)reply_buf);
#endif
        return 0;
      },
      .executed = 0
    };

    res.push_back(get_workload);
    res.push_back(put_workload);
    return res;
  }


  static std::vector<WorkloadDesc> x_workloads(const Addr& server_addr,
                                               W& ycsb,
                                               XCachedClient* x,
                                               RScheduler& r,
                                               RPC& rpc,
                                               char* buf)
  {
    std::vector<WorkloadDesc> res;

    WorkloadDesc get_workload = {
      .name = "get",
      .frequency = ycsba_get_ratio,
      .fn = [&server_addr, &ycsb, x, &r, &rpc](handler_t& h,
                                               char* local_buf, u64 k) -> u64 {
              auto key = ycsb.next_key();
              ASSERT(false);
              //ASSERT(x != nullptr);
        auto v = x->get_direct(key, local_buf, h, r);
        ASSERT(v == ::fstore::SearchCode::Ok);
        //return v.value();
        return (u64)v;
        //return 0;
      },
      .executed = 0
    };

    WorkloadDesc put_workload = {
      .name = "put",
      .frequency = ycsba_put_ratio,
      .fn = [&](handler_t& h, char* local_buf, u64 k) -> u64 {
              ASSERT(false);
#if 1
        char reply_buf[sizeof(ValType)];
        auto key = ycsb.next_key();

        auto& factory = rpc.get_buf_factory();
        // auto send_buf = factory.get_inline_buf();
        // auto send_buf = factory.alloc(sizeof(ValType) + sizeof(GetPayload));
        auto send_buf = local_buf + rpc.reserved_header_sz();

        using namespace ::fstore::server;
        GetPayload req = { .table_id = 0, .key = key };
        Marshal<GetPayload>::serialize_to(req, send_buf);

        auto ret = rpc.call({ .cor_id = r.cur_id(), .dest = server_addr },
                            PUT_ID,
                            { .send_buf = send_buf,
                              .len = sizeof(GetPayload) + sizeof(ValType),
                              .reply_buf = reply_buf,
                              .reply_cnt = 1 });

        r.pause_and_yield(h);
        // factory.dealloc(send_buf);
        return *((u64*)reply_buf);
#endif
        return 0;
      },
      .executed = 0
    };

    res.push_back(get_workload);
    res.push_back(put_workload);
    return res;
  }

  static std::vector<WorkloadDesc> x2_workloads(const Addr& server_addr,
                                                W& ycsb,
                                                XCachedClientV2* x,
                                                RScheduler& r,
                                                RPC& rpc,
                                                char* buf)
  {
    std::vector<WorkloadDesc> res;

    WorkloadDesc get_workload = {
      .name = "get",
      .frequency = ycsba_get_ratio,
      .fn = [&server_addr, &ycsb, x, &r, &rpc](handler_t& h,
                                               char* local_buf, u64 k) -> u64 {
              auto key = ycsb.next_key();
              //ASSERT(x != nullptr);
              //ASSERT(false);
        auto v = x->get_direct(key, local_buf, h, r);
        ASSERT(v == ::fstore::SearchCode::Ok);
        //return v.value();
        return (u64)v;
        //return 0;
      },
      .executed = 0
    };

    WorkloadDesc put_workload = {
      .name = "put",
      .frequency = ycsba_put_ratio,
      .fn = [&](handler_t& h, char* local_buf, u64 k) -> u64 {
#if 1
        char reply_buf[sizeof(ValType)];
        auto key = ycsb.next_key();

        auto& factory = rpc.get_buf_factory();
        // auto send_buf = factory.get_inline_buf();
        // auto send_buf = factory.alloc(sizeof(ValType) + sizeof(GetPayload));
        auto send_buf = local_buf + rpc.reserved_header_sz();

        using namespace ::fstore::server;
        GetPayload req = { .table_id = 0, .key = key };
        Marshal<GetPayload>::serialize_to(req, send_buf);

        auto ret = rpc.call({ .cor_id = r.cur_id(), .dest = server_addr },
                            PUT_ID,
                            { .send_buf = send_buf,
                              .len = sizeof(GetPayload) + sizeof(ValType),
                              .reply_buf = reply_buf,
                              .reply_cnt = 1 });

        r.pause_and_yield(h);
        // factory.dealloc(send_buf);
        return *((u64*)reply_buf);
#endif
        return 0;
      },
      .executed = 0
    };

    res.push_back(get_workload);
    res.push_back(put_workload);
    return res;
  }


  static std::vector<WorkloadDesc> nt_workloads(const Addr& server_addr,
                                                W& ycsb,
                                                NTree* nt,
                                                RScheduler& r,
                                                RPC& rpc)
  {
    std::vector<WorkloadDesc> res;

    WorkloadDesc get_workload = {
      .name = "get",
      .frequency = ycsba_get_ratio,
      .fn = [nt, &ycsb, &r](handler_t& h, char* local_buf,u64 k) -> u64 {
        auto key = ycsb.next_key();
        auto v = nt->get_addr(key, local_buf, r, h).value();
        //auto v = nt->emulate_roundtrip(local_buf, FLAGS_tree_depth, r, h).value();
        return v;
      },
      .executed = 0
    };

    WorkloadDesc put_workload = {
      .name = "put",
      .frequency = ycsba_put_ratio,
      .fn = [&](handler_t& h, char* local_buf, u64 k) -> u64 {
#if 1
        char reply_buf[sizeof(ValType)];
        auto key = ycsb.next_key();

        auto& factory = rpc.get_buf_factory();
        // auto send_buf = factory.get_inline_buf();
        // auto send_buf = factory.alloc(sizeof(ValType) + sizeof(GetPayload));
        auto send_buf = local_buf + rpc.reserved_header_sz();

        using namespace ::fstore::server;
        GetPayload req = { .table_id = 0, .key = key };
        Marshal<GetPayload>::serialize_to(req, send_buf);

        auto ret = rpc.call({ .cor_id = r.cur_id(), .dest = server_addr },
                            PUT_ID,
                            { .send_buf = send_buf,
                              .len = sizeof(GetPayload) + sizeof(ValType),
                              .reply_buf = reply_buf,
                              .reply_cnt = 1 });

        r.pause_and_yield(h);
        // factory.dealloc(send_buf);
        return *((u64*)reply_buf);
#endif
        return 0;
      },
      .executed = 0
    };

    res.push_back(get_workload);
    res.push_back(put_workload);
    return res;
  }

  static std::vector<WorkloadDesc> nt_hybrid_workloads(const Addr& server_addr,
                                                        W& ycsb,
                                                        NTree* nt,
                                                        RScheduler& r,
                                                       RPC& rpc,u64 seed)
  {
    //ASSERT(false);
    std::vector<WorkloadDesc> res;
    util::FastRandom hybrid_random(seed);

    WorkloadDesc get_workload = {
      .name = "get",
      .frequency = ycsba_get_ratio,
      .fn = [&server_addr, &ycsb, nt, &r, &rpc, &hybrid_random](
                                                                handler_t& h, char* local_buf, u64 k) -> u64 {
        auto key = ycsb.next_key();

        auto select = hybrid_random.next() % 100;
        if (select >= FLAGS_hybrid_ratio) {
          char reply_buf[sizeof(ValType)];
          auto key = ycsb.next_key();

          auto& factory = rpc.get_buf_factory();
          auto send_buf = local_buf + rpc.reserved_header_sz();

          using namespace ::fstore::server;
          GetPayload req = { .table_id = 0, .key = key };
          Marshal<GetPayload>::serialize_to(req, send_buf);

          auto ret = rpc.call({ .cor_id = r.cur_id(), .dest = server_addr },
                              GET_ID,
                              { .send_buf = send_buf,
                                .len = sizeof(GetPayload),
                                .reply_buf = reply_buf,
                                .reply_cnt = 1 });

          r.pause_and_yield(h);
          // factory.dealloc(send_buf);
          return *((u64*)reply_buf);
        } else {
          // use RDMA
          auto v = nt->get_addr(key, local_buf, r, h).value();
          return v;
        }
      },
      .executed = 0
    };

    WorkloadDesc put_workload = {
      .name = "put",
      .frequency = ycsba_put_ratio,
      .fn = [&](handler_t& h, char* local_buf, u64 k) -> u64 {
#if 1
        char reply_buf[sizeof(ValType)];
        auto key = ycsb.next_key();

        auto& factory = rpc.get_buf_factory();
        // auto send_buf = factory.get_inline_buf();
        // auto send_buf = factory.alloc(sizeof(ValType) + sizeof(GetPayload));
        auto send_buf = local_buf + rpc.reserved_header_sz();

        using namespace ::fstore::server;
        GetPayload req = { .table_id = 0, .key = key };
        Marshal<GetPayload>::serialize_to(req, send_buf);

        auto ret = rpc.call({ .cor_id = r.cur_id(), .dest = server_addr },
                            PUT_ID,
                            { .send_buf = send_buf,
                              .len = sizeof(GetPayload) + sizeof(ValType),
                              .reply_buf = reply_buf,
                              .reply_cnt = 1 });

        r.pause_and_yield(h);
        // factory.dealloc(send_buf);
        return *((u64*)reply_buf);
#endif
        return 0;
      },
      .executed = 0
    };

    res.push_back(get_workload);
    res.push_back(put_workload);
    return res;
    }

  static std::vector<WorkloadDesc> hybrid_workloads(const Addr& server_addr,
                                                    W& ycsb,
                                                    FClient* fc,
                                                    RScheduler& r,
                                                    RPC& rpc,
                                                    char* buf,
                                                    u64 seed)
  {
    std::vector<WorkloadDesc> res;
    util::FastRandom hybrid_random(seed);

    WorkloadDesc get_workload = {
      .name = "get",
      .frequency = ycsba_get_ratio,
      .fn = [&server_addr, &ycsb, fc, &r, &rpc, &hybrid_random](
                                                                handler_t& h, char* local_buf, u64 k) -> u64 {
        auto key = ycsb.next_key();

        auto select = hybrid_random.next() % 100;
        if (select >= FLAGS_hybrid_ratio) {
          char reply_buf[sizeof(ValType)];
          auto key = ycsb.next_key();

          auto& factory = rpc.get_buf_factory();
          // auto send_buf = factory.get_inline_buf();
          // auto send_buf = factory.alloc(sizeof(ValType) +
          // sizeof(GetPayload));
          auto send_buf = local_buf + rpc.reserved_header_sz();

          using namespace ::fstore::server;
          GetPayload req = { .table_id = 0, .key = key };
          Marshal<GetPayload>::serialize_to(req, send_buf);

          auto ret = rpc.call({ .cor_id = r.cur_id(), .dest = server_addr },
                              GET_ID,
                              { .send_buf = send_buf,
                                .len = sizeof(GetPayload),
                                .reply_buf = reply_buf,
                                .reply_cnt = 1 });

          r.pause_and_yield(h);
          // factory.dealloc(send_buf);
          return *((u64*)reply_buf);
        } else {
          // use RDMA
          // auto send_buf = (char
          // *)AllocatorMaster<>::get_thread_allocator()
          //->alloc(10 * sizeof(Leaf));
          auto predict = fc->get_predict(key);

          // TODO: not check the return status (std::get<0>(res)) of the call
          auto v = std::get<1>(fc->get_addr(key, predict, local_buf, r, h));
          // AllocatorMaster<>::get_thread_allocator()->free(send_buf);
          return v;
        }
      },
      .executed = 0
    };

    WorkloadDesc put_workload = {
      .name = "put",
      .frequency = ycsba_put_ratio,
      .fn = [&](handler_t& h, char* local_buf, u64 k) -> u64 {
#if 1
        char reply_buf[sizeof(ValType)];
        auto key = ycsb.next_key();

        auto& factory = rpc.get_buf_factory();
        // auto send_buf = factory.get_inline_buf();
        // auto send_buf = factory.alloc(sizeof(ValType) + sizeof(GetPayload));
        auto send_buf = local_buf + rpc.reserved_header_sz();

        using namespace ::fstore::server;
        GetPayload req = { .table_id = 0, .key = key };
        Marshal<GetPayload>::serialize_to(req, send_buf);

        auto ret = rpc.call({ .cor_id = r.cur_id(), .dest = server_addr },
                            PUT_ID,
                            { .send_buf = send_buf,
                              .len = sizeof(GetPayload) + sizeof(ValType),
                              .reply_buf = reply_buf,
                              .reply_cnt = 1 });

        r.pause_and_yield(h);
        // factory.dealloc(send_buf);
        return *((u64*)reply_buf);
#endif
        return 0;
      },
      .executed = 0
    };

    res.push_back(get_workload);
    res.push_back(put_workload);
    return res;
  }
  };

} // namespace bench

} // namespace fstore
