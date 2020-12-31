#pragma once

#include <mutex>

#include "../../x_ml/src/xmodel.hh"
#include "../../xkv_core/src/lib.hh"
#include "../../xutils/xy_data.hh"

#include "./sample_trait.hh"

namespace xstore {

namespace xcache {

using namespace xstore::xkv;
using namespace xstore::xml;
using namespace xstore::util;

/*!
  This file implements a traininer for training the second-layer submodel,
  defined in ../../x_ml/src/xmodel.hh
 */
template<typename KeyType>
struct XMLTrainer
{
  // the trainer may be trained in a (background) thread
  std::mutex guard;

  // This model is responsible for predicting [start_key, end_key]
  KeyType start_key = KeyType::max();
  KeyType end_key = KeyType::min();

  /*! seqs which used to define whether this model need to retrain
    the model will train if and only if h_watermark > l_watermark
    FIXME: currently not handle cases when h_watermark/l_watermark overflows
  */
  usize l_watermark = 0;
  usize h_watermark = 0;

  usize nkeys_update = 0;

  /*! 'name' is used to record the train results of this trainer
    if verbose is enabled, the trained result together with the cdf will
    be dumped to "*name*_trained.py" and "*name*_cdf.py", respectively.
   */
  std::string name = "trainer";

  /*********************/

  void set_name(const std::string& n) { this->name = n; }

  auto update_key(const KeyType& k)
  {
    std::lock_guard<std::mutex> lock(this->guard);

    //    if (this->start_key > k || this->end_key < k) {
    // this key is newly added key
    this->nkeys_update += 1;
    //    }

    this->start_key = std::min(this->start_key, k);
    this->end_key = std::max(this->end_key, k);

    // update the watermark to notify retrain
    if (!this->need_train()) {
      this->h_watermark += 1;
    }
  }

  auto need_train() -> bool { return this->h_watermark > this->l_watermark; }

  /*!
    core training method
    \note: assumption this method will only be called in a single-threaded
    context
  */
  template<class IT, template<typename> class S, template<typename> class SubML>
  auto train(typename IT::KV& kv,
             S<KeyType>& s,
             update_func f = default_update_func)
    -> Arc<XSubModel<SubML, KeyType>>
  {
    auto iter = IT::from(kv);
    return train_w_it<IT, S, SubML>(iter, kv, s, f);
  }

  template<class IT, template<typename> class S, template<typename> class SubML>
  auto train_w_it(IT& iter,
                  typename IT::KV& kv,
                  S<KeyType>& s,
                  update_func f = default_update_func,
                  bool verbose = false) -> Arc<XSubModel<SubML, KeyType>>
  {
    DefaultSample<KeyType> shrink;
    return train_w_it_w_shrink<IT, S, DefaultSample, SubML>(
      iter, kv, s, shrink, f);
  }

  template<class IT, template<typename> class S>
  inline auto snapshpot_train_labels(IT& iter,
                                     typename IT::KV& kv,
                                     S<KeyType>& s)
    -> std::pair<std::vector<KeyType>, std::vector<u64>>
  {
    // take a snapshot of the start/end key
    KeyType s_key;
    KeyType e_key;
    {
      std::lock_guard<std::mutex> lock(this->guard);
      s_key = this->start_key;
      e_key = this->end_key;
    }

    std::vector<KeyType> train_set;
    std::vector<u64> train_label;

    for (iter.seek(s_key, kv); iter.has_next(); iter.next()) {
      if (iter.cur_key() > e_key) {
        break;
      }
      s.add_to(iter.cur_key(), iter.opaque_val(), train_set, train_label);
    }
    s.finalize(train_set, train_label);
    return std::make_pair(train_set, train_label);
  }

  /*!
    Similar to train_w_it, but shrink the training-set according to Shrink
   */
  template<class IT,
           template<typename>
           class S,
           template<typename>
           class Shrink,
           template<typename>
           class SubML>
  auto train_w_it_w_shrink(IT& iter,
                           typename IT::KV& kv,
                           S<KeyType>& s,
                           Shrink<KeyType>& ss,
                           update_func f = default_update_func,
                           bool verbose = false)
    -> Arc<XSubModel<SubML, KeyType>>
  {
    // TODO: model should be parameterized
    // XSubModel<SubML, KeyType> model;
    auto model = std::make_shared<XSubModel<SubML, KeyType>>();

    if (!this->need_train()) {
      return model;
    }

    // take a snapshot of the start/end key
    const auto& tl = this->snapshpot_train_labels<IT, S>(iter, kv, s);
    const auto train_set = std::get<0>(tl);
    const auto train_label = std::get<1>(tl);

    // 2. shinrk the train-set
    std::vector<KeyType> s_train_set;
    std::vector<u64> s_train_label;
    for (uint i = 0; i < train_set.size(); i++) {
      ss.add_to(train_set[i], train_label[i], s_train_set, s_train_label);
    }
    ss.finalize(s_train_set, s_train_label);
    // 3. train the model
#if 1
    model->train_ml(s_train_set, s_train_label);
    model->cal_error(train_set, train_label, f);
#endif

    if (verbose) {
      this->dump_cdf(train_set, train_label);
      this->dump_trained(train_set, model);
    }

    // train done
    this->l_watermark += 1;

    return model;
  }

  void dump_cdf(const std::vector<KeyType>& train_set,
                const std::vector<u64>& train_label)
  {
    XYData<double, double> cdf;

    for (uint i = 0; i < train_set.size(); ++i) {
      cdf.add(train_set.at(i).to_scalar(), train_label.at(i));
    }
    cdf.finalize().dump_as_np_data("cdf" + this->name + ".py");
  }

  template<template<typename> class SubML>
  void dump_trained(const std::vector<KeyType>& train_set,
                    const Arc<XSubModel<SubML, KeyType>>& model)
  {
    XYData<double, double> trained;
    for (uint i = 0; i < train_set.size(); ++i) {
      trained.add(train_set.at(i).to_scalar(),
                  model->get_point_predict(train_set.at(i)));
    }
    trained.finalize().dump_as_np_data("trained" + this->name + ".py");
  }

  friend std::ostream& operator<<(std::ostream& out, const XMLTrainer& k)
  {
    return out << "Trainer response for keys: " << k.start_key << "~"
               << k.end_key;
  }
}; // namespace xcache

} // namespace xcache
} // namespace xstore
