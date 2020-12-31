#pragma once

#include <torch/torch.h>

#include <unistd.h>

// I need this logging to overwrite torch's logging
#include "../../../deps/r2/src/logging.hh"
#include "../../../deps/r2/src/utils/rdtsc.hh"

#include "../../../deps/r2/src/timer.hh"

#include "../ml_trait.hh"

#include "../../../xutils/xy_data.hh"

#include "./op_util.hh"

namespace xstore {

namespace xml {

using namespace ::xstore::util;

/*!
  DNN must implements `torch::nn::Module`

  Due to the slow inferene of mainstream ML libraries,
  user has to to provide two network, one major for training (DNN)
  and one dual for inference (DualNN).
  We provide example of how to do so in
  /path/to/xstore/benchs/data_dist/server/log_nn.hh,
  which we implement DualNN using MKL.
 */
template<template<usize> class XNN,
         template<usize>
         class DualNN,
         usize D,
         typename Key>
struct NN : MLTrait<NN<XNN, DualNN, D, Key>, Key>
{
  const bool verbose = true;
  XNN<D> core;
  DualNN<D> dual;
  NN() = default;

  auto predict_impl(const Key& key) -> double
  {
#if 0 
    // use dual   
    auto ret = dual.fast_forward(key);
    return ret;
#else
    auto f = key.to_feature_float();
    auto input = torch::from_blob(f.data(), { 1, D }).clone();
    auto ret = core.forward(input);
    return ret.item().toDouble();
#endif
  }

  void train_impl(std::vector<Key>& train_data,
                  std::vector<u64>& train_label,
                  int step = 1)
  {
    XYData<double, double> cdf;
    XYData<Key, double> feature;
    XYData<double, double> trained;

    //    torch::optim::Adagrad opt(core.parameters(),
    //                              torch::optim::AdagradOptions(0.1));

    torch::optim::Adam opt(core.parameters(),
                           torch::optim::AdamOptions(0.001).weight_decay(0.01));

    /*
    LogNormal dataset uses this training parameters
    torch::optim::Adam opt(core.parameters(),
                               torch::optim::AdamOptions(0.1).weight_decay(0.1));
    */
    std::vector<float> train_set;
    std::vector<float> labels;

    for (uint i = 0; i < train_data.size(); ++i) {
      auto f = train_data.at(i).to_feature();
      for (auto d : f) {
        train_set.push_back(d);
      }
      labels.push_back(train_label.at(i));

      if (verbose) {
        cdf.add(train_data.at(i).to_scalar(), train_label.at(i));
        feature.add(train_data.at(i), train_label.at(i));
      }
    }
    auto real_t_set =
      torch::from_blob(train_set.data(),
                       { static_cast<int>(train_data.size()), D })
        .clone();
    auto l = torch::from_blob(labels.data(),
                              { static_cast<int>(train_data.size()), 1 })
               .clone();
    //    real_t_set = real_t_set.transpose(0, 1);
    // FIXME: the nn training parameters are fixed right now
    const usize nepoch = 40000;

    auto best_loss = std::numeric_limits<float>::max();
    int patience = 0;

    for (int64_t epoch = 0; epoch < nepoch; ++epoch) {
      core.zero_grad();

      torch::Tensor real_output = core.forward(real_t_set);
      torch::Tensor loss = torch::mse_loss(real_output, l);
      loss.backward();
      opt.step();
      if (epoch % 1000 == 0) {
        LOG(2) << "epoch: " << epoch << "|" << nepoch << " loss: " << loss;
      }
      if (loss.item().toFloat() < best_loss) {
        best_loss = loss.item().toFloat();
        patience = 0;
      } else {
        patience += 1;
        //        if (loss.item().toFloat() < 120) {
        //          break;
        //        }
        if (patience > 10 && epoch >= nepoch) {
          LOG(4) << "warning: patience exceed w loss: " << loss;
          break;
        }
      }
      if (loss.item().toFloat() < 10) {
        break;
      }
    }
    LOG(4) << "loss after train:"
           << torch::mse_loss(core.forward(real_t_set), l) << " for "
           << train_data.size() << " points";
    torch::Tensor real_output = core.forward(real_t_set);

    if (verbose) {
      for (auto& k : train_data) {
        auto predict = this->predict(k);
        trained.add(k.to_scalar(), predict);
      }
      cdf.finalize().dump_as_np_data("cdf.py");
      feature.finalize().dump_as_np_data("feature.py");
      trained.finalize().dump_as_np_data("trained.py");
    }

    //  setup dual
    this->dual.set(this->core);
  }

  auto serialize_impl() -> std::string { return "top"; }

  auto serialize_to_file_impl(const std::string& name) -> std::string
  {
    this->core.serialize(name);
    return name;
  }

  void from_serialize_impl(const ::xstore::string_view& data)
  {
    //    this->core.from_serialize(data);
    ASSERT(false) << "not implemented";
  }

  void from_file_impl(const ::xstore::string_view& name)
  {
    // FIXME: currently hard-coded
    this->core.from_file("xfirst");
    this->dual.set(this->core);
  }
};
} // namespace xml
} // namespace xstore
