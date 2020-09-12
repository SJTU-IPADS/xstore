#pragma once

#include "../../x_ml/src/lib.hh"
#include "../../xkv_core/src/iter_trait.hh"

#include "./sample_trait.hh"

namespace xstore {

namespace xcache {

using namespace xstore::xkv;

/*!
  The dispatcher will route a key to some number between [1,n),
  using an ML model.
 */
template <template <typename> class ML, typename KeyType> struct Dispatcher {
  ML<KeyType> model;
  const usize dispatch_num;
  usize       up_bound = 0;

  explicit Dispatcher(const usize &dn) : dispatch_num(dn) {
    ASSERT(dispatch_num > 0) << " dispatch number must be larger than zero";
  }

  template <class... Args>
  Dispatcher(const usize &dn, Args... args) : dispatch_num(dn), model(args...) {}

  void model_from_serialize(const std::string &s) {
    this->model.from_serialize(s);
  }

  auto predict(const KeyType &key,const usize &max) -> usize {
    auto res = static_cast<int>(this->model.predict(key));
    if (res < 0) {
      res = 0;
    }
    if (res >= max) {
      res = this->dispatch_num - 1;
    } else {
      res = static_cast<float>(res) / max * this->dispatch_num;
    }
    return static_cast<usize>(res);
  }

  /*!
    Train the dispatcher with the KV
    The KV must implements the KeyIterTrait.
    S must implement the SampleTrait.

    \ret how many keys trained
   */
  template <class IT, class S>
  auto train(typename IT::KV &kv, S &s) -> usize {
    std::vector<KeyType> train_set;
    std::vector<u64>     train_label;

    // 1. fill in the train_set, train_label
    auto it = IT::from(kv);
    // FIXME: assumes the totoal KVS is smaller than usize's limit
    usize count = 0;
    for (it.begin(); it.has_next(); it.next()) {
      // filter whether we should ad the key
      s.add_to(it.cur_key(), count, train_set, train_label);
      count += 1;
      up_bound = std::max<usize>(up_bound, count);
    }

    // 2. train the ml model
    this->model.train(train_set, train_label);

    return train_set.size();
  }

  // the default sampler will train all the keys
  template <class IT>
  auto default_train(typename IT::KV &kv) -> usize {
    DefaultSample<KeyType> s;
    return this->train<IT, DefaultSample<KeyType>>(kv, s);
  }
};

} // namespace xcache

} // namespace xstore
