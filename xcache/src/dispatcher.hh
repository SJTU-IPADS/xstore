#pragma once

#include "../../x_ml/src/lib.hh"
#include "../../xkv_core/src/iter_trait.hh"

namespace xstore {

namespace xcache {

using namespace r2;
using namespace xstore::xkv;

/*!
  The dispatcher will route a key to some number between [1,n),
  using an ML model.
 */
template <class ML> struct Dispatcher {
  ML model;
  const usize dispatch_num;

  explicit Dispatcher(const usize &dn) : dispatch_num(dn) {
    ASSERT(dispatch_num > 0) << " dispatch number must be larger than zero";
  }

  template <class... Args>
  Dispatcher(const usize &dn, Args... args) : Dispatcher(dn), model(args...) {}

  auto predict(const u64 &key, const u64 &max) -> usize {
    auto res = static_cast<int>(this->model.predict(key));
    if (res < 0) {
      res = 0;
    }
    if (res > max) {
      res = this->dispatch_num - 1;
    } else {
      res = static_cast<float>(res) / max * this->dispatch_num;
    }
    return static_cast<usize>(res);
  }

  /*!
    Train the dispatcher with the KV
    The KV must implements the KeyIterTrait
   */
  // TODO: we need a sample mechanism
  template <class IT>
  auto train(IT::KV &kv) {
    std::vector<KeyType> train_set;
    std::vector<u64>     train_label;

    // 1. fill in the train_set, train_label
  }
};

} // namespace xcache

} // namespace xstore
