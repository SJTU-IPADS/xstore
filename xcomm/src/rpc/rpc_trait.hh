#pragma once

// Result<> to record whether the op is done
#include "../../../deps/rlib/core/result.hh"

// Memblock, which abstract away a raw pointer
#include "../../../deps/r2/src/mem_block.hh"


namespace xstore {

namespace rpc {

template <class Derived> class RPCTrait {
public:
};
} // namespace rpc
} // namespace xstore
