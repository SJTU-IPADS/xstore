#pragma once

#include "../../../deps/r2/src/sshed.hh"

#include "./async_rw_trait.hh"

#include "./local_rw_op.hh"

namespace xstore {

namespace xcomm {

namespace rw {

using namespace rdmaio;

struct AsyncLocalRWOp : public AsyncReadWriteTrait<AsyncLocalRWOp>
{
  AsyncLocalRWOp() = default;
  auto read_impl(const MemBlock& src, const MemBlock& dest, R2_ASYNC)
    -> Result<>
  {
    return LocalRWOp().read(src, dest);
  }
};
} // namespace rw
} // namespace xcomm
} // namespace xstore