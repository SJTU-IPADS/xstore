#pragma once

#include "./ml_trait.hh"

namespace xstore {

namespace xml {

/*!
  LR in a compact form, which uses float (4-byte) to store ml parameters
 */
struct CompactLR {
  float w;
  float b;
};

} // namespace xml

} // namespace xstore
