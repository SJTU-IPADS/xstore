/**
 * This file provides common utilities and definiation of RLib
 */

#pragma once

#include <cstdint>
#include <tuple>
#include <infiniband/verbs.h>

#include "option.hh"

#include "logging.hpp"

namespace rdmaio
{

// some constants definiations
// connection status
enum IOStatus
{
  SUCC = 0,
  TIMEOUT = 1,
  WRONG_ARG = 2,
  ERR = 3,
  NOT_READY = 4,
  UNKNOWN = 5,
  WRONG_ID = 6,
  WRONG_REPLY = 7,
  NOT_CONNECT = 8,
  EJECT = 9,
  REPEAT_CREATE = 10
};

using u64 = uint64_t;
using u32 = uint32_t;
using u16 = uint16_t;
using i64 = int64_t;
using u8 = uint8_t;
using i8 = int8_t;
using usize = unsigned int;

/**
 * Programmer can register simple request handler to RdmaCtrl.
 * The request can be bound to an ID.
 * This function serves as the pre-link part of the system.
 * So only simple function request handling is supported.
 * For example, we use this to serve the QP and MR information to other nodes.
 */
enum RESERVED_REQ_ID
{
  REQ_RC = 0,
  REQ_UD = 1,
  REQ_UC = 2,
  REQ_MR = 3,
  FREE = 4
};

enum
{
  MAX_INLINE_SIZE = 64
};

/**
 * We use TCP/IP to identify the machine,
 * since RDMA requires an additional naming mechanism.
 */
using MacID = std::tuple<std::string, int>;
inline MacID make_id(const std::string &ip, int port)
{
  return std::make_tuple(ip, port);
}

class QPDummy
{
 public:
  bool valid() const
  {
    return qp_ != nullptr && cq_ != nullptr;
  }
  struct ibv_qp *qp_ = nullptr;
  struct ibv_cq *cq_ = nullptr;
  struct ibv_cq *recv_cq_ = nullptr;
}; // a placeholder for a dummy class

typedef struct __attribute__ ((packed))
{
  u64 subnet_prefix;
  u64 interface_id;
  u32 local_id;
} qp_address_t;

struct __attribute__ ((packed)) QPAttr
{
  QPAttr(const qp_address_t &addr, u64 lid, u64 psn, u64 port_id, u64 qpn = 0, u64 qkey = 0) : addr(addr), lid(lid), qpn(qpn), psn(psn), port_id(port_id)
  {
  }
  QPAttr() {}
  qp_address_t addr;
  u64 lid;
  u64 psn;
  u64 port_id;
  u64 qpn;
  u64 qkey;
};

} // namespace rdmaio

#include "marshal.hpp"
#include "pre_connector.hpp"
