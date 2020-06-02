#pragma once

#include <string.h>

#include "./rw_trait.hh"

namespace xstore {

namespace xcomm {

namespace rw {

class LocalRWOp : public ReadWriteTrait<LocalRWOp> {
public:
  LocalRWOp() = default;

  auto read_impl(MemBlock &src, MemBlock &dest) -> Result<> {
    if (src.sz > dest.sz) {
      // size not match
      return ::rdmaio::Err();
    }
    memcpy(dest.mem_ptr, src.mem_ptr, dest.sz);
    return ::rdmaio::Ok();
  }

  // a local op's write share the same implementation as the read
  auto write_impl(MemBlock &src, MemBlock &dest) -> Result<> {
    return this->read_impl(dest, src);
  }
};
}

} // namespace xcomm

} // namespace xstore
