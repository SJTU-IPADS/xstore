#pragma once

#include <gflags/gflags.h>

#include "r2/src/rpc/rpc.hpp"

#include "marshal.hpp"
#include "mem_region.hpp"

#include "../internal/tables.hpp"
#include "../proto.hpp"

namespace fstore {

namespace server {

RegionManager&
global_memory_region();
RdmaCtrl&
global_rdma_ctrl();

DECLARE_int64(threads);

class ControlHandlers
{
public:
  /*!
    Fetch the meta data of the server
   */
  static void meta_server(RPC& rpc,
                          const Req::Meta& ctx,
                          const char* msg,
                          u32 payload)
  {

    auto page_region = global_memory_region().get_region("page");
    auto page_start = global_memory_region()
                        .get_rdma_addr((char*)BlockPager<Leaf>::blocks)
                        .value();
    ServerMeta meta = {
      .global_rdma_addr =
        reinterpret_cast<u64>(global_memory_region().base_mem_),
      .page_addr = page_start,
      .page_area_sz = page_region.size,
      .num_threads = FLAGS_threads,
    };

    auto& factory = rpc.get_buf_factory();

    char* reply_buf = factory.get_inline();
    Marshal<ServerMeta>::serialize_to(meta, reply_buf);
    rpc.reply(ctx, reply_buf, sizeof(ServerMeta));
  }

  /*!
    Meta data of the table
   */
  static void meta_table(RPC& rpc,
                         const Req::Meta& ctx,
                         const char* msg,
                         u32 payload)
  {}

  static void delete_qp(RPC& rpc,
                        const Req::Meta& ctx,
                        const char* msg,
                        u32 payload)
  {
    QPRequest req = Marshal<QPRequest>::deserialize(msg, payload);
    global_rdma_ctrl().qp_factory.delete_rc_qp(req.id);
  }

  static void query_table_id(RPC&, const Req::Meta& ctx, const char*, u32) {}

  static void register_all(RPC& rpc)
  {
    rpc.register_callback(META_TAB, meta_table);
    rpc.register_callback(META_SERVER, meta_server);
    rpc.register_callback(DELETE_QP, delete_qp);
  }
};

} // server

} // fstore
