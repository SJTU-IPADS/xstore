#pragma once

#include "common.hpp"
#include "rnic.hpp"
#include "simple_rpc.hpp"
#include "util.hpp"

#include <cerrno>
#include <cstring>
#include <map>
#include <mutex>
#include <assert.h>

namespace rdmaio {

class MemoryFlags
{
public:
  MemoryFlags() = default;

  MemoryFlags& set_flags(int flags)
  {
    protection_flags = flags;
    return *this;
  }

  int get_flags() const { return protection_flags; }

  MemoryFlags& clear_flags() { return set_flags(0); }

  MemoryFlags& add_local_write()
  {
    protection_flags |= IBV_ACCESS_LOCAL_WRITE;
    return *this;
  }

  MemoryFlags& add_remote_write()
  {
    protection_flags |= IBV_ACCESS_REMOTE_WRITE;
    return *this;
  }

  MemoryFlags& add_remote_read()
  {
    protection_flags |= IBV_ACCESS_REMOTE_READ;
    return *this;
  }

private:
  int protection_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ |
                         IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_ATOMIC;
};

/**
 * Simple wrapper over ibv_mr struct
 */
class RemoteMemory
{
public:
  RemoteMemory(const char* addr,
               u64 size,
               const RNic& rnic,
               const MemoryFlags& flags)
    : addr(addr)
    , size(size)
  {
    if (rnic.ready()) {
      mr = ibv_reg_mr(rnic.pd, (void*)addr, size, flags.get_flags());

      if (!valid()) {
        RDMA_LOG(4) << "register mr failed at addr: (" << (void*)addr << ","
                    << size << ")"
                    << " with error: " << std::strerror(errno);
        assert(false);
      }
    }
  }

  bool valid() const { return (mr != nullptr); }

  ~RemoteMemory()
  {
    // if(mr != nullptr)
    // ibv_dereg_mr(mr);
  }

  struct __attribute__ ((packed)) Attr
  {
    uintptr_t buf;
    u32 key;
    u64 sz;
  };

  Attr get_attr() const
  {
    auto key = valid() ? mr->rkey : 0;
    return { .buf = (uintptr_t)(addr), .key = key, .sz = size };
  }

private:
  const char* addr = nullptr;
  u64 size;
  ibv_mr* mr = nullptr; // mr in the driver
};                      // class remote memory

/**
 * The MemoryFactor manages all registered memory of the system
 */
class RdmaCtrl;
class RMemoryFactory
{
  friend class RdmaCtrl;

  std::map<int, RemoteMemory*> registered_mrs;
  std::mutex lock;

public:
  RMemoryFactory() = default;
  ~RMemoryFactory()
  {
    for (auto it = registered_mrs.begin(); it != registered_mrs.end(); ++it)
      delete it->second;
  }

  IOStatus register_mr(int mr_id,
                       const char* addr,
                       u64 size,
                       RNic& rnic,
                       const MemoryFlags flags = MemoryFlags())
  {
    std::lock_guard<std::mutex> lk(this->lock);

    if (registered_mrs.find(mr_id) != registered_mrs.end()) {
      return WRONG_ID;
    }
    RDMA_ASSERT(rnic.ready()) << "rnic is not ready";

    registered_mrs.insert(
      std::make_pair(mr_id, new RemoteMemory(addr, size, rnic, flags)));

    if (registered_mrs[mr_id]->valid())
      return SUCC;

    registered_mrs.erase(registered_mrs.find(mr_id));
    return ERR;
  }

  static IOStatus fetch_remote_mr(int mr_id,
                                  const MacID& id,
                                  RemoteMemory::Attr& attr,
                                  const Duration_t& timeout = default_timeout)
  {
    SimpleRPC sr(std::get<0>(id), std::get<1>(id));
    if (!sr.valid())
      return ERR;
    Buf_t reply;
    sr.emplace(
      (u8)REQ_MR, Marshal::serialize_to_buf(static_cast<u64>(mr_id)), &reply);
    auto ret =
      sr.execute(sizeof(ReplyHeader) + sizeof(RemoteMemory::Attr), timeout);

    if (ret == SUCC) {
      // further we check the reply header
      ReplyHeader header = Marshal::deserialize<ReplyHeader>(reply);
      if (header.reply_status == SUCC) {
        reply = Marshal::forward(
          reply, sizeof(ReplyHeader), reply.size() - sizeof(ReplyHeader));
        attr = Marshal::deserialize<RemoteMemory::Attr>(reply);
      } else
        ret = static_cast<IOStatus>(header.reply_status);
    }
    return ret;
  }

  IOStatus fetch_local_mr(int mr_id, RemoteMemory::Attr& attr)
  {
    auto mr = get_mr(mr_id);
    if (mr == nullptr) {
      return ERR;
    }
    attr = mr->get_attr();
    return SUCC;
  }

  void deregister_mr(int mr_id)
  {
    std::lock_guard<std::mutex> lk(this->lock);
    auto it = registered_mrs.find(mr_id);
    if (it == registered_mrs.end())
      return;
    delete it->second;
    registered_mrs.erase(it);
  }

  RemoteMemory* get_mr(int mr_id)
  {
    std::lock_guard<std::mutex> lk(this->lock);
    if (registered_mrs.find(mr_id) != registered_mrs.end())
      return registered_mrs[mr_id];
    return nullptr;
  }

private:
  // fetch the MR attribute from the registered mrs
  Buf_t get_mr_attr(u64 id)
  {
    std::lock_guard<std::mutex> lk(this->lock);
    if (registered_mrs.find(id) == registered_mrs.end()) {
      return "";
    }
    auto attr = registered_mrs[id]->get_attr();
    return Marshal::serialize_to_buf(attr);
  }

  /** The RPC handler for the mr request
   * @Input = req:
   * - the attribute of MR the requester wants to fetch
   */
  Buf_t get_mr_handler(const Buf_t& req)
  {
    if (req.size() < sizeof(u64))
      return Marshal::null_reply();

    u64 mr_id;
    bool res = Marshal::deserialize(req, mr_id);
    if (!res)
      return Marshal::null_reply();

    ReplyHeader reply = { .reply_status = SUCC,
                          .reply_payload = sizeof(RemoteMemory::Attr) };

    auto mr = get_mr_attr(mr_id);

    if (mr.size() == 0) {
      reply.reply_status = NOT_READY;
      reply.reply_payload = 0;
    }
    // finally generate the reply
    auto reply_buf = Marshal::serialize_to_buf(reply);
    reply_buf.append(mr);

    return reply_buf;
  }
};

} // end namespace rdmaio
