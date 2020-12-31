#pragma once
// define the DNN with pytorch

//#define C10_USE_GFLAGS
#include <torch/torch.h>

#include "../../x_ml/src/nn/mat.hh"

// I need this logging to overwrite torch's logging
#include "../../deps/r2/src/logging.hh"

#include "../../xutils/print.hh"

namespace xstore {

namespace xml {

template<usize D>
class LogNet : public torch::nn::Module
{
public:
  const int H = 32;
  LogNet()
    : l1(register_module("l1", torch::nn::Linear(D, H)))
    , l4(register_module("l4", torch::nn::Linear(H, 1)))
  {
#if 1
    torch::NoGradGuard no_grad;
    // init the weight
    for (auto& p : this->parameters()) {
      p.fill_(0.001);
      //      p.uniform_(0.0, 1.0);
    }
#endif
  }

  explicit LogNet(const std::string& name)
  {
    // TODO: load from the file
  }
  torch::Tensor forward(torch::Tensor input)
  {
    auto ret = l1(input / 100000000.0);
    ret = torch::sigmoid(ret);
    return l4(ret);
  }

  template<typename Key>
  double fast_forward(const Key& k)
  {
    return 0;
  }

  void serialize(const std::string& name)
  {
    torch::save(l1, name + "_l1.pt");
    torch::save(l4, name + "_l4.pt");
  }

  void from_file(const ::xstore::string_view& name)
  {
#if 0
    for (uint i = 0; i < 5; ++i) {
      try {
        torch::load(l1, std::string(name) + "_l1.pt");
        goto p2;
      } catch (std::exception& e) {
        sleep(1);
      }
      ASSERT(false) << "load fatal error!";
    }

  p2:
    for (uint i = 0; i < 5; ++i) {
      try {
        torch::load(l4, std::string(name) + "_l4.pt");
        goto p3;
      } catch (std::exception& e) {
        sleep(1);
      }
    }
    ASSERT(false) << "load fatal error on l4!";
  p3:
#else
    // FIXME: hard coded because concurrent read from nfs may cause execption
    torch::load(l1, "/home/wxd/" + std::string(name) + "_l1.pt");

    torch::load(l4, "/home/wxd/" + std::string(name) + "_l4.pt");
#endif
    return;
  }

  torch::nn::Linear l1{ nullptr };
  torch::nn::Linear l2{ nullptr };
  torch::nn::Linear l3{ nullptr };
  torch::nn::Linear l4{ nullptr };
};

template<usize D>
class DualLogNet
{
public:
  DualLogNet() = default;

  // FIXME: MKL default uses double for computation
  /*!
    First layer
  */
  std::vector<double> w1;
  std::vector<double> b1;

  /*!
    Second layer
  */
  std::vector<double> w2;
  std::vector<double> b2;

  using Mat = ::xstore::xml::Matrix<double>;

  template<typename K>
  auto fast_forward(K& key) -> double
  {
    auto f = key.to_feature();
    // normalize based on the model
    for (uint i = 0; i < f.size(); ++i) {
      f[i] = f[i] / 100000000.0;
    }
    Mat F(f.data(), 1, f.size());
    Mat W1(w1.data(), f.size(), w1.size());
    auto temp = b1;
    Mat ret(temp.data(), 1, w1.size());

    // temp = w1 * f + b1
    F.man_mult(W1, ret); // res in ret

#if 1
    for (uint i = 0; i < temp.size(); ++i) {
      temp[i] = ::xstore::xml::Op::rough_sigmoid(temp[i]);
      // temp[i] = ::xstore::xml::Op::sigmoid(temp[i]);
    }
#endif

    double res = b2[0];
    Mat W2(w2.data(), w2.size(), 1);
    Mat RES(&res, 1, 1);
    ret.man_mult(W2, RES);

    return res;
  }

  void set(const LogNet<D>& d)
  {
    this->w1.clear();
    this->b1.clear();

    this->w2.clear();
    this->b2.clear();

    { // w1
      torch::Tensor ten = d.l1->named_parameters()["weight"];
      std::vector<float> v(ten.data_ptr<float>(),
                           ten.data_ptr<float>() + ten.numel());
      for (auto e : v) {
        this->w1.push_back(static_cast<double>(e));
      }
    }

    { // b1
      torch::Tensor ten = d.l1->named_parameters()["bias"];
      std::vector<float> v(ten.data_ptr<float>(),
                           ten.data_ptr<float>() + ten.numel());
      for (auto e : v) {
        this->b1.push_back(static_cast<double>(e));
      }
    }

    { // w2
      torch::Tensor ten = d.l4->named_parameters()["weight"];
      std::vector<float> v(ten.data_ptr<float>(),
                           ten.data_ptr<float>() + ten.numel());
      for (auto e : v) {
        this->w2.push_back(static_cast<double>(e));
      }
    }

    { // b2
      torch::Tensor ten = d.l4->named_parameters()["bias"];
      std::vector<float> v(ten.data_ptr<float>(),
                           ten.data_ptr<float>() + ten.numel());
      for (auto e : v) {
        this->b2.push_back(static_cast<double>(e));
      }
    }
  }

  friend std::ostream& operator<<(std::ostream& out, const DualLogNet& mod)
  {
    out << "w1: " << ::xstore::util::vec_slice_to_str(mod.w1, 0, 32);
    return out;
  }
};

} // namespace xml
} // namespace xstore