#pragma once

#include "../server/internal/table.hpp"
#include "../server/proto.hpp"

#include "../src/marshal.hpp"

#include "./xdirect.hh"

#include "../deps/r2/src/rdma/single_op.hpp"

namespace fstore {

using namespace r2::rpc;

class XBoot
{
public:
  RCQP *qp;               // QP for communication
  u64  base_addr;         // RDMA-base addr, for translating remote virtual address to RDMA offset
  u64  submodels_addr;    // submodel-base addr

  XBoot(RCQP* qp, u64 b, u64 s)
    : qp(qp)
    , base_addr(b)
    , submodels_addr(s)
  {}

  inline u64 virt_to_rdma(const u64& v) { return v - base_addr; }

  bool boot_extract_model(std::shared_ptr<SBM> &ret, char *buf, Option<u64> &ext) {
    auto temp = ret->seq;
    r2::compile_fence();
    ret->seq = INVALID_SEQ;
    r2::compile_fence();
    if (!Serializer::extract_submodel_to(buf, *ret,ext)) {
      //LOG(4) << "serialize submodel error: " << i << " [" << rs << " : " << re
      //<< "]";
      //ASSERT(false) << " offset: " << ptr - local_buf << " " << submodel_sz;
      //      ASSERT(false);
      return false;
    }
    r2::compile_fence();
    ret->seq = temp + 2;
    return true;
  }

  //std::shared_ptr<SBM>
  bool update_submodel(std::shared_ptr<SBM> &ret, int idx, char *local_buf, R2_ASYNC) {
    if (ret == nullptr) {
      ret = std::shared_ptr<SBM>(new SBM);
    }

    //auto ret = std::shared_ptr<SBM> (new SBM);
    auto submodel_sz = Serializer::sizeof_submodel<LRModel<>>() + sizeof(u64) + sizeof(u64);

    ::r2::rdma::SROp op(this->qp);
    op.set_read()
      .set_remote_addr(this->virt_to_rdma(submodels_addr + static_cast<u64>(idx) * submodel_sz))
      .set_payload(local_buf, submodel_sz + sizeof(u64) + sizeof(u64));
    auto res = op.execute(R2_ASYNC_WAIT);
    ASSERT(std::get<0>(res) == SUCC);

    //bool sr = Serializer::extract_submodel_to(local_buf, *ret);
    //ASSERT(sr) << "extract error: " << idx;
    Option<u64> ext_addr = {};
    if (*((u64 *)local_buf) == 73) {
      // failed
      //ASSERT(false);
      return true;
    }
    auto rc =  boot_extract_model(ret, local_buf, ext_addr);

    if (idx == 7318994) {
      //LOG(4) << "sanity check page table sz: " << ret->page_table.size();
      //ASSERT(false);
    }

    if (ext_addr) {
      // need to serialize
      if(ret->page_table.size() * sizeof(u64) > 4096){
        LOG(4) << "Ret error: " << ret->min_error << " " << ret->max_error << " " << ret->page_table.size();
        //ASSERT(false);
        return rc;
      }
      op.set_read()
        .set_remote_addr(this->virt_to_rdma(ext_addr.value()))
        .set_payload(local_buf, ret->page_table.size() * sizeof(u64));
      auto res = op.execute(R2_ASYNC_WAIT);
      ASSERT(std::get<0>(res) == SUCC);
      for (uint i = 0;i < ret->page_table.size(); ++i) {
        auto addr = Marshal<u64>::extract_with_inc(local_buf);
        ret->page_table[i] = addr;
      }
    }
    return rc;
  }

  const usize max_read_length = 1024 * 1024 * 16;
  void batch_update_all(std::vector<std::shared_ptr<SBM>> &all, bool update, char *local_buf, R2_ASYNC) {
    ASSERT(all.size() > 0);
    usize submodel_sz = Serializer::sizeof_submodel<LRModel<>>() + sizeof(u64) + sizeof(u64);
    int batch_num = std::min(max_read_length / submodel_sz, 8192u);
    ASSERT(submodel_sz * batch_num <= max_read_length);
    //int batch_num = 1;

    usize fetched = 0;
    for (fetched = 0; fetched < all.size(); fetched += batch_num) {
      this->batch_update(all, fetched, std::min(static_cast<usize>(fetched + batch_num), static_cast<usize>(all.size())) ,
                         update,
                         local_buf, R2_ASYNC_WAIT);
    }

    // fetch the remaining
    if (all.size() > fetched) {
      this->batch_update(all, fetched, all.size(), update, local_buf, R2_ASYNC_WAIT);
    }
  }

  void batch_update(std::vector<std::shared_ptr<SBM>> &all,
                    const usize rs, const usize re,
                    bool update,
                    char *local_buf, R2_ASYNC) {

    ASSERT(rs < re) << rs << " " << re;
    usize submodel_sz = Serializer::sizeof_submodel<LRModel<>>() + sizeof(u64) + sizeof(u64);
    LOG(0) << "updat batch: " << rs << " " << re << " with each submodel sz: "<< submodel_sz << " " << (re- rs);

    ::r2::rdma::SROp op(this->qp);
    op.set_read()
      .set_remote_addr(
        this->virt_to_rdma(submodels_addr + static_cast<u64>(rs) * submodel_sz))
      .set_payload(local_buf, submodel_sz * static_cast<u64>((re - rs)));
    LOG(0) << "read addr: " << submodels_addr + rs * submodel_sz
           << " with payload: " << (submodel_sz * (re - rs));

    auto res = op.execute(R2_ASYNC_WAIT);
    ASSERT(std::get<0>(res) == SUCC);

    char* ptr = local_buf;
#if 1
    for (uint i = rs; i < re; ++i) {
      ASSERT(i < all.size()) << i << " " << rs << " : " << re;

      if (all[i] == nullptr) {
        all[i] = std::shared_ptr<SBM>(new SBM);
      }
      auto ret = all[i];
      if (update) {
        Option<u64> ext = {};
        boot_extract_model(ret, ptr,ext);
        //ASSERT(!ext) << "should use ext " << ret->page_table.size();

        if (ext) {
          if (ret->page_table.size() * sizeof(u64) > 4096) {
            LOG(4) << "Ret error: " << ret->min_error << " " << ret->max_error
                   << " " << ret->page_table.size();
            ASSERT(false);
          }
          op.set_read()
            .set_remote_addr(this->virt_to_rdma(ext.value()))
            .set_payload(local_buf, ret->page_table.size() * sizeof(u64));
          auto res = op.execute(R2_ASYNC_WAIT);
          ASSERT(std::get<0>(res) == SUCC);
          for (uint i = 0; i < ret->page_table.size(); ++i) {
            auto addr = Marshal<u64>::extract_with_inc(local_buf);
            ret->page_table[i] = addr;
          }
        }
      }
      ptr += submodel_sz;

      // sanity check
      ASSERT(ret->min_error <= ret->max_error);
      ASSERT(ret->page_table.size() != 0);
    }
#endif
  }

  static std::pair<XBoot, std::shared_ptr<XDirectTopLayer>> bootstrap_xcache(
    u64 table_id,
    RPC& rpc,
    const Addr& server_addr,
    RCQP* qp,
    R2_ASYNC)
  {
    char reply_buf[1024];
    memset(reply_buf, 0, 1024);

    auto& factory = rpc.get_buf_factory();
    auto send_buf = factory.alloc(128);

    // first we fetch the Meta data
    Marshal<TableModel>::serialize_to({ .id = table_id }, send_buf);

    auto ret = rpc.call({ .cor_id = R2_COR_ID(), .dest = server_addr },
                        MODEL_META,
                        { .send_buf = send_buf,
                          .len = sizeof(TableModel),
                          .reply_buf = reply_buf,
                          .reply_cnt = 1 });
    R2_PAUSE;
    factory.dealloc(send_buf);

    // parse the reply
    ModelMeta meta = Marshal<ModelMeta>::extract(reply_buf);
    LOG(0) << "sanity check meta at client : " << meta.dispatcher_sz << " "
           << meta.submodel_buf_addr << " " << meta.num_submodel << " "
           << meta.max_addr;

    // first alloc the XBoot
    XBoot boot(qp, meta.base_addr, meta.submodel_buf_addr);
    auto  x = std::make_shared<XDirectTopLayer>(std::string(reply_buf + sizeof(ModelMeta), meta.dispatcher_sz),
                                                meta.num_submodel, meta.max_addr);
    return std::make_pair(boot, x);
  }
};
}
