#pragma once

#include "common.hpp"

#include <iostream>
#include <vector>

namespace rdmaio {

struct DevIdx {
  uint dev_id;
  uint port_id;

  friend std::ostream& operator<<(std::ostream& os, const DevIdx& i) {
    return os << "{" << i.dev_id << ":" << i.port_id << "}";
  }
};

class RNicInfo;
class RemoteMemory;
class RCQP;
class UDQP;
class QPUtily;

// The RNIC handler
class RNic {
 public:
  RNic(DevIdx idx,int gid = 0) :
      id(idx),
      ctx(open_device(idx)),
      pd(alloc_pd(ctx)),
      lid(fetch_lid(ctx,idx)),
      addr(query_addr(gid)) {
  }

  bool ready() const {
    return (ctx != nullptr) && (pd != nullptr) && lid >= 0;
  }

  ~RNic() {
    // pd must he deallocaed before ctx
    if(pd != nullptr) {
      ibv_dealloc_pd(pd);
      //RDMA_VERIFY(INFO,ibv_dealloc_pd(pd) == 0)
      //<< "failed to dealloc pd at device " << id
      //  << "; w error " << strerror(errno);
    }
    if(ctx != nullptr) {
      //RDMA_VERIFY(INFO,ibv_close_device(ctx) == 0)
      //<< "failed to close device " << id;
      ibv_close_device(ctx);
    }

  }
  // members and private helper functions
 public:
  const DevIdx id;
  struct ibv_context *ctx = nullptr;
  struct ibv_pd      *pd  = nullptr;

  const qp_address_t  addr;
  const int           lid = -1;

 private:
  struct ibv_context *open_device(DevIdx idx) {

    ibv_context *ret = nullptr;

    int num_devices = -1;
    struct ibv_device **dev_list = ibv_get_device_list(&num_devices);
    if(idx.dev_id > num_devices || idx.dev_id < 0) {
      RDMA_LOG(WARNING) << "wrong dev_id: " << idx << "; total " << num_devices <<" found";
      goto ALLOC_END;
    }

    RDMA_ASSERT(dev_list != nullptr);
    ret = ibv_open_device(dev_list[idx.dev_id]);
    if(ret == nullptr) {
      RDMA_LOG(WARNING) << "failed to open ib ctx w error: " << strerror(errno)
                        << "; at devid " << idx;
      goto ALLOC_END;
    }
 ALLOC_END:
    if(dev_list != nullptr)
      ibv_free_device_list(dev_list);
    return ret;
  }

  struct ibv_pd *alloc_pd(struct ibv_context *c) {
    if(c == nullptr) return nullptr;
    auto ret = ibv_alloc_pd(c);
    if(ret == nullptr) {
      RDMA_LOG(WARNING) << "failed to alloc pd w error: " << strerror(errno);
    }
    return ret;
  }

  int fetch_lid(ibv_context *ctx,DevIdx idx) {
    ibv_port_attr port_attr;
    auto rc = ibv_query_port(ctx, idx.port_id, &port_attr);
    if(rc >= 0)
      return port_attr.lid;
    return -1;
  }

  qp_address_t query_addr(uint8_t gid_index = 0) const {

    ibv_gid gid;
    ibv_query_gid(ctx,id.port_id,gid_index,&gid);

    qp_address_t addr {
      .subnet_prefix = gid.global.subnet_prefix,
      .interface_id  = gid.global.interface_id,
      .local_id      = gid_index
    };
    return addr;
  }

  friend class RNicInfo;
  friend class RemoteMemory;
  friend class QPUtily;
  friend class RCQP;
  friend class UDQP;
}; // end class RNic

/**
 * The following static methods describes the parameters of the devices.
 */
class RNicInfo {
  using Vec_t = std::vector<DevIdx>;
 public:
  static Vec_t query_dev_names() {
    Vec_t res;
    int num_devices;
    struct ibv_device **dev_list = ibv_get_device_list(&num_devices);

    for(uint i = 0;i < num_devices;++i) {
      RNic rnic({.dev_id = i,.port_id = 1 /* a dummy value*/});
      if(rnic.ready()) {
        ibv_device_attr attr;
        auto rc = ibv_query_device(rnic.ctx, &attr);

        if (rc)
          continue;
        for(uint port_id = 1;port_id <= attr.phys_port_cnt;++port_id) {
          res.push_back({.dev_id = i,.port_id = port_id});
        }
      } else
        RDMA_ASSERT(false);
    }
    if(dev_list != nullptr)
      ibv_free_device_list(dev_list);
    return res;
  }

}; // end class RNicInfo

} // end namespace rdmaio
