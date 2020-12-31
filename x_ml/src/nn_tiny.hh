#pragma once

// DNN implementation using tiny-dnn

#include <unistd.h>

#include "../deps/tiny-dnn/tiny_dnn/tiny_dnn.h"
#include "./ml_trait.hh"

#include "../../deps/r2/src/timer.hh"

namespace xstore {
namespace xml {

template<typename Key>
struct XNN : MLTrait<XNN<Key>, Key>
{

  using NNN = ::tiny_dnn::network<::tiny_dnn::sequential>;
  NNN net;
  /*!
    2 means that we convert a u64 to 2 float
   */

  using act_type = ::tiny_dnn::activation::sigmoid;
  //  using act_type = ::tiny_dnn::activation::leaky_relu;
  explicit XNN(NNN& n)
    : net(std::move(n))
  {}

  auto predict_impl(const Key& key) -> double
  {
    r2::Timer t;
    auto res = net.predict(key.to_feature_float())[0];
    LOG(4) << "predict using time: " << t.passed_msec();
    sleep(1);
    return res;
  }

  void train_impl(std::vector<Key>& train_data,
                  std::vector<u64>& train_label,
                  int step = 1)
  {
    using namespace tiny_dnn;

    std::vector<vec_t> real_train_set;
    std::vector<vec_t> labels;
    for (uint i = 0; i < train_data.size(); ++i) {
      vec_t v;
      for (auto f : train_data.at(i).to_feature_float()) {
        v.push_back(f);
      }
      real_train_set.push_back(v);
      labels.push_back(vec_t({ static_cast<float>(train_label[i]) }));
      // LOG(4) << "train key: " << real_train_set[i][0] << " "
    }

    this->net.weight_init(weight_init::constant(0.1));
    this->net.bias_init(weight_init::constant(0.5));

    //    using opt = adagrad;
    //    using opt = ::tiny_dnn::adam;
    // using opt = nesterov_momentum;
    // adagrad optimizer;
    // nesterov_momentum optimizer;

    adagrad optimizer;
    // train 120 epochs
    auto ret = net.train<mse, ::tiny_dnn::adagrad>(
      optimizer, real_train_set, labels, real_train_set.size(), 300);

    ASSERT(ret);
    LOG(0) << "Train done using training-set:" << real_train_set.size();
    r2::compile_fence();
  }

  auto serialize_impl() -> std::string
  {
    ASSERT(false) << "not implemented";
    return "";
  }

  void from_serialize_impl(const std::string& data)
  {
    ASSERT(false) << "not implemented";
  }

  auto serialize_to_file_impl(const std::string& name) -> std::string
  {
    ASSERT(false) << "not implemented";
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
    ASSERT(false) << " not implemented";
  }
};
} // namespace xml
} // namespace xstore
