#pragma once

#include "r2/src/rpc/rpc.hpp"

#include "mem_region.hpp"
#include "marshal.hpp"

#include "../internal/tables.hpp"
#include "../proto.hpp"

namespace fstore {

namespace server {

extern Tables global_table;
RegionManager &global_memory_region();

/*!
  Handlers for learned index bootstrap tasks,
  such as: fetch model based address and mapping table.
 */
class LRHandlers {
 public:
  // fetch the B+Tree meta data
  static void tree_root(RPC &rpc, const Req::Meta &ctx, const char *msg,u32 payload) {

    TableModel req = Marshal<TableModel>::deserialize(msg, payload);

    auto& factory = rpc.get_buf_factory();
    char* reply_buf = factory.alloc(sizeof(TableModelConfig) + 64);
    ASSERT(reply_buf != nullptr);

    auto& tab = global_table.get_table(req.id);
    tab.guard->lock();

    TreeRoot *root = (TreeRoot *)reply_buf;
    root->addr = (u64)tab.data.root;
    root->base = global_memory_region().get_startaddr();

    Inner *inner = (Inner *)tab.data.root;
    LOG(4) << "sanity check root key num : " << inner->num_keys;

    tab.guard->unlock();

    auto ret = rpc.reply(ctx, reply_buf, sizeof(TreeRoot) + 64);
    ASSERT(ret == SUCC);
  }

  static void xcache_meta(RPC& rpc,
                          const Req::Meta& ctx,
                          const char* msg,
                          u32 payload) {
    //LOG(4) << "answer xcache meta";
    TableModel req = Marshal<TableModel>::deserialize(msg, payload);

    auto& factory = rpc.get_buf_factory();
    char* reply_buf = factory.alloc(sizeof(ModelMeta) + 64);
    ASSERT(reply_buf != nullptr);

    auto& tab = global_table.get_table(req.id);

    ASSERT(tab.xcache != nullptr) << "xcache null for id: "<< req.id;
    ASSERT(tab.submodel_buf != nullptr);

    ModelMeta meta
    {
      .dispatcher_sz = LRModel<>::buf_sz(),
      .submodel_buf_addr = (u64)(tab.submodel_buf),
      .num_submodel = tab.xcache->sub_models.size(),
      .max_addr = tab.xcache->max_addr,
      .base_addr = global_memory_region().get_startaddr()
    };
    LOG(0) << "sanity check meta: " << meta.dispatcher_sz << " "
           << meta.submodel_buf_addr << " " << meta.num_submodel << " "
           << meta.max_addr;

    Marshal<ModelMeta>::serialize_to(meta, reply_buf);

    auto buf = tab.xcache->dispatcher.serialize();
    ASSERT(buf.size() < 64);
    memcpy(reply_buf + sizeof(ModelMeta), buf.data(), buf.size());

    LOG(0) << "meta disp parameters:" << tab.xcache->dispatcher.w << " "
           << tab.xcache->dispatcher.b;

    auto ret =
      rpc.reply(ctx, reply_buf, sizeof(ModelMeta) + buf.size());
    ASSERT(ret == SUCC);
  }

  /*!
      \note: we need to ensure that all server has created its meta data through
      to an RDMA memory region
     */
    static void model_meta(RPC& rpc,
                           const Req::Meta& ctx,
                           const char* msg,
                           u32 payload)
  {

    TableModel req = Marshal<TableModel>::deserialize(msg,payload);

    auto &factory = rpc.get_buf_factory();
    char *reply_buf = factory.alloc(sizeof(TableModelConfig) + 64);
    ASSERT(reply_buf != nullptr);

    auto &tab = global_table.get_table(req.id);
    //LOG(2) << "[XX] in one model critical section with lock for table: " << (usize)req.id << " " << &tab.guard;
    tab.guard->lock();
    //LOG(2) << "[XX] really in critical section";
#if 1
    TableModelConfig config = {
      .first_buf_addr = global_memory_region().get_rdma_addr(std::get<0>(tab.first_model_buf)).value(),
      .first_buf_sz = std::get<1>(tab.first_model_buf),
      .second_buf_addr = global_memory_region().get_rdma_addr(std::get<0>(tab.second_model_buf)).value(),
      .second_buf_sz = std::get<1>(tab.second_model_buf),
      .total_keys = tab.sc->index->sorted_array_size,
      .key_n      = tab.sc->index->rmi.key_n,
      .mega_addr  = global_memory_region().get_rdma_addr(std::get<0>(tab.mega_buf)).value(),
      .mega_sz    = std::get<1>(tab.mega_buf),
      .tree_depth = tab.data.depth
    };
    ASSERT(std::get<1>(tab.first_model_buf) != 0);
    ASSERT(std::get<1>(tab.second_model_buf) != 0);
    ASSERT(config.first_buf_addr != 0);
    ASSERT(config.second_buf_addr != 0);
    Marshal<TableModelConfig>::serialize_to(config,reply_buf);
    //LOG(2) << "[XX] exit model fetcher critical section with: " << config.first_buf_addr << " " << config.first_buf_sz;
#endif
    tab.guard->unlock();
    auto ret = rpc.reply(ctx,reply_buf,sizeof(TableModelConfig) + 64);
    ASSERT(ret == SUCC);
    /*
      XD: fix me: current has memory leakage problem.
      But otherwise, UD's buffer will be poluted, so we just allocate a dedicated buf.
      Luckily, this reply is on the control path of the execution, so it possibly may not leak too many memory.
      Will be fixed later.
     */
    //factory.dealloc(reply_buf);
  }

  static void register_all(RPC &rpc) {
    //rpc.register_callback(MODEL_META,model_meta);
    rpc.register_callback(MODEL_META, xcache_meta);
    rpc.register_callback(TREE_META,tree_root);
  }
};

} // server

} //fstore
