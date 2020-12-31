#pragma once

// Result<> to record whether the op is done
#include "../../../deps/rlib/core/result.hh"

// Memblock, which abstract away a raw pointer
#include "../../../deps/r2/src/mem_block.hh"
#include "../../../deps/r2/src/libroutine.hh"

namespace xstore {

namespace xcomm {

namespace rw {

using namespace rdmaio;
using namespace r2;

// abstract interface
template <class Derived> class AsyncReadWriteTrait {
public:
  // read content from src -> dest
  auto read(const MemBlock &src, const MemBlock &dest, R2_ASYNC) -> Result<> {
    return static_cast<Derived *>(this)->read_impl(src, dest, R2_ASYNC_WAIT);
  }
};

} // namespace rw
} // namespace xcomm

} // namespace xstore
