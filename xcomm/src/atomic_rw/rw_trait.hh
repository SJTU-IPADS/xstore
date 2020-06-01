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
template <class Derived>
class ReadWriteTrait {
public:
  // read content from dest -> src
  Result<> read(Memblock &src, Memblock &dest) {
    return static_cast<Derived *>(this)->read_impl(src,dest);
  }

  // write content from src -> dest
  Result<> write(Memblock &src, Memblock &dest) {
    return static_cast<Derived *>(this)->read_impl(src,dest);
  }
};

}
} // namespace xcomm

} // namespace xstore
