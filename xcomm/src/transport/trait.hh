#pragma once

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
};

// Recv Trait
// ST state for send_trait, because receive trait will return the current send session
template <class Derived, class ST> struct RTrait {
public:
  void begin() { reinterpret_cast<Derived *>(this)->begin_impl(); }

  void next() { reinterpret_cast<Derived *>(this)->next_impl(); }

  auto has_msgs() -> bool {
    return reinterpret_cast<Derived *>(this)->has_msgs_impl();
  }

  auto cur_msg() -> MemBlock {
    return reinterpret_cast<Derived *>(this)->cur_msg_impl();
  }

  auto reply_entry() -> ST {
    return reinterpret_cast<Derived *>(this)->reply_entry_impl();
  }
};

} // namespace transport
} // namespace xstore
