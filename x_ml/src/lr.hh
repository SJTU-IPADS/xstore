#pragma once

#include <mkl.h>
#include <mkl_lapacke.h>

#include "./ml_trait.hh"

namespace xstore {

namespace xml {

/*!
  Core LR Model, leverage MKL for training
 */
struct LR {
  // TODO: impl
  double w;
  double b;
};

} // namespace xml
} // namespace xstore
