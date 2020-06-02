#pragma once

#include <string.h>

#include "./rw_trait.hh"

namespace xstore {

namespace xcomm {

namespace rw {

class LocalRWOp : public ReadWriteTrait<LocalRWOp> {
public:
  LocalRWOp() = default;

  auto read_impl(const MemBlock &src, const MemBlock &dest) -> Result<> {
    if (src.sz > dest.sz) {
      // size not match
      return ::rdmaio::Err();
    }
    // memcpy version
    memcpy(dest.mem_ptr, src.mem_ptr, dest.sz);
    return ::rdmaio::Ok();
  }

};
}

} // namespace xcomm

} // namespace xstore
