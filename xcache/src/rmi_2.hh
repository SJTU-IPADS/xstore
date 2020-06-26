#pragma once

#include "./dispatcher.hh"
#include "./statics.hh"

#include "./submodel_trainer.hh"

namespace xstore {

namespace xcache {

/*!
  This file implements a two-layer RMI learned index described in the original
  learned index paper: The case for learned index structure.
  We hard-coded to use a two-layer index.
 */

template <typename DispatchML, typename SubML> struct LocalTwoRMI {
  Dispatcher<DispatchML> first_layer;
  std::vector<XSubModel<SubML>> second_layer;

  explicit LocalTwoRMI(const usize &num_sec)
      : second_layer(num_sec), first_layer(num_sec) {}

  // TODO: add multiple arg init of fist layer

  // training methods
  template <class IT, class S>
  auto train_first(typename IT::KV &kv, S &s) -> usize {
    return this->first_layer.train(kv, s);
  }

  template <class IT> auto default_train_first(typename IT::KV &kv) -> usize {
    return this->first_layer.template default_train<IT>(kv);
  }

  auto select_sec_model(const KeyType &k) -> usize {
    return first_layer.predict(k, first_layer.up_bound);
  }

  auto get_predict_range(const KeyType &k) -> std::pair<int, int> {
    auto model = this->select_sec_model(k);
    return this->second_layer[model].get_predict_range(k);
  }

  template <class IT>
  auto dispatch_keys_to_trainers(typename IT::KV &kv)
      -> std::vector<XMLTrainer> {
    std::vector<XMLTrainer> trainers(this->second_layer.size());
    auto it = IT::from(kv);
    for (it.begin(); it.has_next(); it.next()) {
      auto model = this->select_sec_model(it.cur_key());
      //      LOG(4) << "update m:" << model << " w key: "<< it.cur_key();
      ASSERT(model >= 0 && model < trainers.size()) << "invalid model n: " << model
                                                    << "; max: " << this->first_layer.up_bound;
      trainers[model].update_key(it.cur_key());
    }
    return trainers;
  }

  template <class IT, class S>
  auto train_second_models(typename IT::KV &kv, S &s, Statics &statics,
                           update_func f = default_update_func) {
    auto it = IT::from(kv);

    auto trainers = this->dispatch_keys_to_trainers<IT>(kv);
    // dispatch done
    for (uint i = 0; i < second_layer.size(); ++i) {
      this->second_layer[i] = trainers[i].template train<IT, S, SubML>(kv, s, f);
    }
    // done
  }
};

} // namespace xcache
} // namespace xstore
