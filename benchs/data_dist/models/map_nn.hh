#pragma once

// NN used for predicting the map dataset

// define the DNN with pytorch

//#define C10_USE_GFLAGS
#include <torch/torch.h>

// I need this logging to overwrite torch's logging
#include "../../deps/r2/src/logging.hh"

namespace xstore {

namespace xml {

template<usize D>
class MapNet : public torch::nn::Module
{
public:
  const int H = 4;
  MapNet()
    : l1(register_module("l1", torch::nn::Linear(D, H)))
    , l2(register_module("l2", torch::nn::Linear(H, H)))
    , l3(register_module("l3", torch::nn::Linear(H, H)))
    , l4(register_module("l4", torch::nn::Linear(H, 1)))
  {}

  auto forward(torch::Tensor input) -> torch::Tensor
  {
    auto ret = l1(input);
    ret = torch::leaky_relu(ret);
#if 1
    ret = l2(ret);

    ret = torch::leaky_relu(ret);
#endif
    return l4(ret);
  }
  torch::nn::Linear l1{ nullptr };
  torch::nn::Linear l2{ nullptr };
  torch::nn::Linear l3{ nullptr };
  torch::nn::Linear l4{ nullptr };
};

template<usize D>
class DualMapNet
{
public:
  // !! to be fixed later
  MapNet<D>* dual = nullptr;
  DualMapNet() = default;
  template<typename K>
  auto fast_forward(K& key) -> double
  {
    ASSERT(this->dual != nullptr);
    auto f = key.to_feature();
    auto input = torch::from_blob(f.data(), { 1, D }).clone();
    return this->dual->forward(input)[0].item().toDouble();
  }
  void set(const MapNet<D>& d)
  {
    LOG(4)
      << "warning: DualMapNet not implemented; fallback to torch for inference";
    // !!  XD: see the above comment
    this->dual = (MapNet<D>*)(&d);
  }
};

} // namespace xml
} // namespace xstore