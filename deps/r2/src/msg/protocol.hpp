#pragma once

#include <functional>
#include <memory>
#include <sstream>

#include "rlib/common.hpp"

#include "../common.hpp"

namespace r2
{

struct Addr
{
  u32 mac_id : 16;
  u32 thread_id : 16;

  inline u64 to_u32() const
  {
    return *((u32 *)this);
  }

  inline void from_u32(u32 res)
  {
    *((u32 *)(this)) = res;
  }

  std::string to_str() const
  {
    std::ostringstream oss;
    oss << "address's mac_id: " << mac_id
        << "; address's thread: " << thread_id;
    return oss.str();
  }
};

using Addr_id_t = u32;

struct IncomingMsg
{
  char *msg;
  int size;
  Addr from;
};

/**
 * An iterator class so that application can handle in-coming messages
 */
class IncomingIter
{
public:
  virtual IncomingMsg next() = 0;
  virtual bool has_next() = 0;
  virtual ~IncomingIter(){};
};

using Iter_p_t = std::unique_ptr<IncomingIter>;

/**
 * The message can have multiple implementations.
 * So we use a virtual class to identify which functions must be implemented
 * for a msg protocol.
 */
class MsgProtocol
{
public:
  /**
   * connect to the remote end point
   * opt: an optional field which can be used by different message implementations
   */
  virtual rdmaio::IOStatus connect(const Addr &addr, const rdmaio::MacID &id, int opt = 0) = 0;

  /*!
    Internal connect to a client, given the connect_info
   */
  virtual rdmaio::IOStatus connect_from_incoming(const Addr &addr, const rdmaio::Buf_t &connect_info)
  {
    return rdmaio::SUCC;
  }

  virtual rdmaio::Buf_t get_my_conninfo()
  {
    return rdmaio::Buf_t("");
  }

  /*!
   * Dis-connect from a client
   */
  virtual void disconnect(const Addr &addr)
  {
  }

  virtual rdmaio::IOStatus send_async(const Addr &addr, const char *msg, int size) = 0;

  virtual rdmaio::IOStatus flush_pending() = 0;

  virtual int padding() const
  {
    return 0;
  }

  // send a message to a destination
  rdmaio::IOStatus send(const Addr &addr, const char *msg, int size)
  {
    auto ret = send_async(addr, msg, size);
    if (likely(ret == rdmaio::SUCC))
      ret = flush_pending();
    return ret;
  }

  // poll all in-coming msgs
  using msg_callback_t = std::function<void(const char *, int size, const Addr &addr)>;
  virtual int poll_all(const msg_callback_t &f) = 0;

  virtual Iter_p_t get_iter() = 0;
};

} // end namespace r2
