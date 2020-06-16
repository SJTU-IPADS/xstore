#pragma once

// Result<> to record whether the op is done
#include "../../../deps/rlib/core/result.hh"

// Memblock, which abstract away a raw pointer
#include "../../../deps/r2/src/mem_block.hh"

namespace xstore {

namespace rpc {

// TODO: not implemented
template <class Derived> class CallTrait {
public:

};

template <class Derived> class RecvTrait {
public:

};


} // namespace rpc
} // namespace xstore
