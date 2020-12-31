#pragma once

// Result<> to record whether the op is done
#include "../../../deps/rlib/core/result.hh"

// Memblock, which abstract away a raw pointer
#include "../../../deps/r2/src/mem_block.hh"

namespace xstore {

namespace xcomm {

namespace rw {

using namespace rdmaio;
using namespace r2;

// abstract interface
template<class Derived>
class ReadWriteTrait
{
public:
  // read content from src -> dest
  auto read(const MemBlock& src, const MemBlock& dest) -> Result<>
  {
    return static_cast<Derived*>(this)->read_impl(src, dest);
  }

  // write content from dest -> src
  auto write(const MemBlock& src, const MemBlock& dest) -> Result<>
  {
    return this->read(dest, src);
  }
};

}
} // namespace xcomm

} // namespace xstore
