#pragma once

#include "./dispatcher.hh"

#include "../../x_ml/src/xmodel.hh"

namespace xstore {

namespace xcache {

/*!
  This file implements a two-layer RMI learned index described in the original
  learned index paper: The case for learned index structure.
  We hard-coded to use a two-layer index.
 */

template <typename DispatchML, typename SubML> struct TwoRMI {
  Dispatcher<DispatchML> first_layer;
  std::vector<XSubModel<SubML>> second_layer;

  explicit TwoRMI(const usize &num_sec) : second_layer(num_sec) {
  }
};

} // namespace xcache
} // namespace xstore
