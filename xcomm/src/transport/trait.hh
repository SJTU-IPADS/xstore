#pragma once

#include <unordered_map>

// Result<> to record whether the op is done
#include "../../../deps/rlib/core/result.hh"

// Memblock, which abstract away a raw pointer
#include "../../../deps/r2/src/mem_block.hh"

namespace xstore {

namespace transport {

using namespace rdmaio;
using namespace r2;

// Send Trait
template <class Derived> struct STrait {
public:
  // send
  template <class... Args>
  auto connect(const std::string &host, Args ... args) -> Result<> {
    return reinterpret_cast<Derived *>(this)->connect_impl(host,args...);
  }

  auto send(const MemBlock &msg, const double &timeout = 1000000)
      -> Result<std::string> {
    return reinterpret_cast<Derived *>(this)->send_impl(msg, timeout);
  }

  // RDMA should pass a lkey so that the RNIC is able to access the message
  // leave an explict function to all it
  auto send_w_key(const MemBlock &msg, const u32 &lkey,const double &timeout = 1000000)
      -> Result<std::string> {
    return reinterpret_cast<Derived *>(this)->send_w_key_impl(msg, lkey, timeout);
  }

  auto get_connect_data() -> r2::Option<std::string> {
    return reinterpret_cast<Derived *>(this)->get_connect_data_impl();
  }
};

// Recv Trait
// ST state for send_trait, because receive trait will return the current send session
template <class Derived, class ST> struct RTrait {
public:
  void begin() { reinterpret_cast<Derived *>(this)->begin_impl(); }

  void end() {
    reinterpret_cast<Derived *>(this)->end_impl();
  }

  void next() { reinterpret_cast<Derived *>(this)->next_impl(); }

  auto has_msgs()->bool {
    return reinterpret_cast<Derived *>(this)->has_msgs_impl();
  }

  auto cur_msg() -> MemBlock {
    return reinterpret_cast<Derived *>(this)->cur_msg_impl();
  }

  auto cur_session_id() -> u32 {
    return reinterpret_cast<Derived *>(this)->cur_session_id_impl();
  }

  // legacy API
  auto reply_entry() -> ST {
    return reinterpret_cast<Derived *>(this)->reply_entry_impl();
  }
};

template <class Derived, class SendTrait, class RecvTrait>
struct SessionManager {
  std::unordered_map<u32, std::unique_ptr<SendTrait>> incoming_sesions;

  // TODO: how to delete?
  auto add_new_session(const u32 &id, const MemBlock &raw_connect_data, RecvTrait &recv_trait) -> Result<> {
    return reinterpret_cast<Derived *>(this)->add_impl(id, raw_connect_data, recv_trait);
  }
};

} // namespace transport
} // namespace xstore
