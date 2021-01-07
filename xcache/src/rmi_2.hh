#pragma once

#include <map>

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

template<template<typename> class DispatchML,
         template<typename>
         class SubML,
         typename KeyType>
struct LocalTwoRMI
{
  template<typename T>
  using FirstML = DispatchML<T>;

  Dispatcher<DispatchML, KeyType> first_layer;
  std::vector<Arc<XSubModel<SubML, KeyType>>> second_layer;

  using Sub = XSubModel<SubML, KeyType>;

  explicit LocalTwoRMI(const usize& num_sec)
    : second_layer(num_sec)
    , first_layer(num_sec)
  {}

  template<class... Args>
  LocalTwoRMI(const usize& dn, Args... args)
    : second_layer(dn)
    , first_layer(dn, args...)
  {}

  LocalTwoRMI(const ::xstore::string_view& s, bool load_from_file = false)
    : first_layer(s, load_from_file)
  {}

  // TODO: add multiple arg init of fist layer

  auto num_subs() const -> usize { return second_layer.size(); }

  // training methods
  template<class IT, template<typename> class S>
  auto train_first(typename IT::KV& kv, S<KeyType>& s) -> usize
  {
    return this->first_layer.template train<IT, S>(kv, s);
  }

  template<class IT>
  auto default_train_first(typename IT::KV& kv) -> usize
  {
    return this->first_layer.template default_train<IT>(kv);
  }

  auto load_first_from_file(const std::string& name, const u64& up)
  {
    this->first_layer.load_from_file(name, up);
  }

  auto select_sec_model(const KeyType& k) -> usize
  {
    auto res = first_layer.predict(k, first_layer.up_bound);
    if (res >= second_layer.size()) {
      res = second_layer.size() - 1;
    }
    if (res < 0) {
      return 0;
    }
    return res;
  }

  auto get_predict_range(const KeyType& k) -> std::pair<int, int>
  {
    auto model = this->select_sec_model(k);
    return this->second_layer[model]->get_predict_range(k);
  }

  auto get_point_predict(const KeyType& k) -> int
  {
    auto model = this->select_sec_model(k);
    return this->second_layer.at(model)->get_point_predict(k);
  }

  auto get_predict_raw(const KeyType& k) -> double
  {
    auto model = this->select_sec_model(k);
    return this->second_layer.at(model)->get_predict_raw(k);
  }

  template<class IT>
  auto dispatch_keys_to_trainers(typename IT::KV& kv)
    -> std::vector<XMLTrainer<KeyType>>
  {
    std::vector<XMLTrainer<KeyType>> trainers(this->second_layer.size());
    auto it = IT::from(kv);
    for (it.begin(); it.has_next(); it.next()) {
      auto model = this->select_sec_model(it.cur_key());

      ASSERT(model >= 0 && model < trainers.size())
        << "invalid model n: " << model
        << "; max: " << this->first_layer.up_bound;
      trainers[model].update_key(it.cur_key());
    }
    return trainers;
  }

  auto emplace_one_second_model(const ::xstore::string_view& s)
  {
    this->second_layer.emplace_back(new Sub(s));
  }

  template<class IT, template<typename> class S>
  auto train_second_models(typename IT::KV& kv,
                           S<KeyType>& s,
                           Statics& statics,
                           update_func f = default_update_func)
  {
    auto it = IT::from(kv);

    auto trainers = this->dispatch_keys_to_trainers<IT>(kv);
    // dispatch done
    for (uint i = 0; i < second_layer.size(); ++i) {
      this->second_layer[i] =
        trainers[i].template train<IT, S, SubML>(kv, s, f);
    }
    // done
  }

  static auto sub_serialize_sz() -> usize
  {
    return SubML<KeyType>::serialize_sz();
  }

  /* Some utilities to dump all training results of XCACHE to a python readable
   * format */
  // dump the result of the first layer prediction
  template<class IT>

  auto dump_first(typename IT::KV& kv)
  {
    auto it = IT::from(kv);

    XYData<double, double> all;
    for (; it.has_next(); it.next()) {
      auto key = it.cur_key();
      all.add(key.to_scalar(), this->first_layer.predict_raw(key));
    }
    all.finalize().dump_as_np_data("first.py");
  }

  template<class IT>

  auto dump_cdf(typename IT::KV& kv)
  {
    auto it = IT::from(kv);

    XYData<double, double> all;
    usize counter = 0;
    for (; it.has_next(); it.next(), counter++) {
      auto key = it.cur_key();
      all.add(key.to_scalar(), counter);
    }
    all.dump_as_np_data("cdf.py");
  }

  auto calculate_model_offset() -> std::vector<usize>
  {
    std::vector<usize> model_offset;
    model_offset.push_back(0);
    for (uint i = 1; i < this->second_layer.size(); ++i) {
      model_offset.push_back(model_offset[model_offset.size() - 1] +
                             this->second_layer[i - 1]->max);
    }
    return model_offset;
  }

  template<class IT>
  auto dump_all(typename IT::KV& kv)
  {
    XYData<double, double> all;

    auto it = IT::from(kv);

    std::vector<usize> model_offset = this->calculate_model_offset();

    for (; it.has_next(); it.next()) {
      auto key = it.cur_key();
      auto m = this->select_sec_model(key);
      auto p = this->get_predict_raw(key);

      all.add(key.to_scalar(), p + model_offset[m]);
    }
    //    all.finalize().dump_as_np_data("all.py");
    all.dump_as_np_data("all.py");
  }

  // very slow function call
  // to optimize later
  template<class IT>
  auto dump_labels(typename IT::KV& kv,
                   const std::vector<std::map<KeyType, u64>>& labels)
  {
    XYData<double, double> all;
    ASSERT(labels.size() == this->second_layer.size());

    auto it = IT::from(kv);
    auto model_offset = this->calculate_model_offset();

    for (; it.has_next(); it.next()) {
      auto key = it.cur_key();
      auto m = this->select_sec_model(key);
      const auto& mapping = labels.at(m);
      ASSERT(mapping.find(key) != mapping.end())
        << "failed to find key: " << key << " @ model: " << m;
      const auto l = mapping.find(key)->second;
      all.add(key.to_scalar(), l + model_offset[m]);
    }
    all.dump_as_np_data("labels.py");
  }
};

} // namespace xcache
} // namespace xstore
