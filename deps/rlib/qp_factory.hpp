#pragma once

#include "rc.hpp"
#include "ud.hpp"

#include <map>

namespace rdmaio {

class RdmaCtrl;
class QPFactory
{
  friend class RdmaCtrl;

public:
  QPFactory() = default;

  ~QPFactory()
  {
    for (auto it = rc_qps.begin(); it != rc_qps.end(); ++it)
      delete it->second;
    for (auto it = ud_qps.begin(); it != ud_qps.end(); ++it)
      delete it->second;
  }

  bool register_rc_qp(u64 id, RCQP* qp)
  {
    std::lock_guard<std::mutex> lk(this->lock);
    if (rc_qps.find(id) != rc_qps.end())
      return false;
    rc_qps.insert(std::make_pair(id, qp));
    return true;
  }

  bool delete_rc_qp(u64 id)
  {
    std::lock_guard<std::mutex> lk(this->lock);
    auto it = rc_qps.find(id);
    if (it != rc_qps.end()) {
      rc_qps.erase(it);
      delete it->second;
    }
  }

  bool register_ud_qp(u64 id, UDQP* qp)
  {
    std::lock_guard<std::mutex> lk(this->lock);
    if (ud_qps.find(id) != ud_qps.end())
      return false;
    ud_qps.insert(std::make_pair(id, qp));
    return true;
  }

  RCQP* get_rc_qp(u64 id)
  {
    std::lock_guard<std::mutex> lk(this->lock);
    if (rc_qps.find(id) != rc_qps.end())
      return rc_qps[id];
    return nullptr;
  }

  enum TYPE
  {
    RC = REQ_RC,
    UD = REQ_UD
  };

  static IOStatus fetch_qp_addr(TYPE type,
                                u64 qp_id,
                                const MacID& id,
                                QPAttr& attr,
                                const Duration_t& timeout = default_timeout)
  {
    SimpleRPC sr(std::get<0>(id), std::get<1>(id));
    if (!sr.valid())
      return ERR;
    Buf_t reply;
    sr.emplace(
      (u8)type, Marshal::serialize_to_buf(static_cast<u64>(qp_id)), &reply);
    auto ret = sr.execute(sizeof(ReplyHeader) + sizeof(QPAttr), timeout);

    if (ret == SUCC) {
      // further we check the reply header
      ReplyHeader header = Marshal::deserialize<ReplyHeader>(reply);
      if (header.reply_status == SUCC) {
        reply = Marshal::forward(
          reply, sizeof(ReplyHeader), reply.size() - sizeof(ReplyHeader));
        attr = Marshal::deserialize<QPAttr>(reply);
      } else {

        ret = static_cast<IOStatus>(header.reply_status);
      }
    }
    return ret;
  }

private:
  std::map<u64, RCQP*> rc_qps;
  std::map<u64, UDQP*> ud_qps;

  // TODO: add UC QPs

  std::mutex lock;

  Buf_t get_rc_addr(u64 id)
  {
    std::lock_guard<std::mutex> lk(this->lock);
    if (rc_qps.find(id) == rc_qps.end()) {
      return "";
    }
    auto attr = rc_qps[id]->get_attr();
    return Marshal::serialize_to_buf(attr);
  }

  Buf_t get_ud_addr(u64 id)
  {
    std::lock_guard<std::mutex> lk(this->lock);
    if (ud_qps.find(id) == ud_qps.end()) {
      return "";
    }
    auto attr = ud_qps[id]->get_attr();
    return Marshal::serialize_to_buf(attr);
  }

  /** The RPC handler for the qp request
   * @Input = req:
   * - the address of QP the requester wants to fetch
   */
  Buf_t get_rc_handler(const Buf_t& req)
  {

    if (req.size() < sizeof(u64))
      return Marshal::null_reply();

    u64 qp_id;
    bool res = Marshal::deserialize(req, qp_id);
    if (!res)
      return Marshal::null_reply();

    ReplyHeader reply = { .reply_status = SUCC,
                          .reply_payload = sizeof(qp_address_t) };

    auto addr = get_rc_addr(qp_id);

    if (addr.size() == 0) {
      reply.reply_status = NOT_READY;
      reply.reply_payload = 0;
    }

    // finally generate the reply
    auto reply_buf = Marshal::serialize_to_buf(reply);
    reply_buf.append(addr);
    return reply_buf;
  }

  Buf_t get_ud_handler(const Buf_t& req)
  {

    if (req.size() < sizeof(u64)) {
      return Marshal::null_reply();
    }

    u64 qp_id;
    RDMA_ASSERT(Marshal::deserialize(req, qp_id));

    ReplyHeader reply = { .reply_status = SUCC,
                          .reply_payload = sizeof(qp_address_t) };

    auto addr = get_ud_addr(qp_id);

    if (addr.size() == 0) {
      reply.reply_status = NOT_READY;
      reply.reply_payload = 0;
    }

    // finally generate the reply
    auto reply_buf = Marshal::serialize_to_buf(reply);
    reply_buf.append(addr);
    return reply_buf;
  }

}; //

} // end namespace rdmaio
