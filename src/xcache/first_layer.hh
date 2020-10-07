#pragma once

#include "./sub_model.hh"

namespace fstore {

template<typename Model0>
class DispatchLayer
{
  Model0 ml;

  explicit DispatchLayer(int num_sub)
    : sub_models(num_sub)
  {}
};

}
