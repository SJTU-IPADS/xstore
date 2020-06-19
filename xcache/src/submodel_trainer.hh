#pragma once

#include <mutex>

#include "../../x_ml/src/xmodel.hh"
#include "../../xkv_core/src/lib.hh"

#include "./sample_trait.hh"

namespace xstore {

namespace xcache {

using namespace xstore::xkv;
using namespace xstore::xml;

/*!
  This file implements a traininer for training the second-layer submodel,
  defined in ../../x_ml/src/xmodel.hh
 */
struct XMLTrainer {
  // the trainer may be trained in a (background) thread
  std::mutex guard;

  // This model is responsible for predicting [start_key, end_key]
  KeyType start_key = std::numeric_limits<KeyType>::max();
  KeyType end_key = 0;

  /*! seqs which used to define whether this model need to retrain
    the model will train if and only if h_watermark > l_watermark
    FIXME: currently not handle cases when h_watermark/l_watermark overflows
  */
  usize l_watermark = 0;
  usize h_watermark = 0;

  /*********************/

  auto update_key(const KeyType &k) {
    std::lock_guard<std::mutex> lock(this->guard);
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
  template <class IT, class S, class SubML>
  auto train(typename IT::KV &kv, S &s, update_func f = default_update_func)
      -> XSubModel<SubML> {

    // TODO: model should be parameterized
    XSubModel<SubML> model;

    if (!this->need_train()) {
      return model;
    }

    // take a snapshot of the start/end key
    KeyType s_key;
    KeyType e_key;
    {
      std::lock_guard<std::mutex> lock(this->guard);
      s_key = this->start_key;
      e_key = this->end_key;
      this->l_watermark += 1;
    }

    std::vector<u64> train_set;
    std::vector<u64> train_label;

    // 1. fill in the train_set/label
    auto iter = IT::from(kv);
    // base is used to algin the labels start from 0
    u64 base = 0;
    if (iter.has_next()) {
      // valid
      ASSERT(iter.cur_key() == s_key);
      base = iter.opaque_val();
    }

    for (iter.seek(s_key); iter.has_next(); iter.next()) {
      s.add_to(iter.cur_key(), iter.opaque_val() - base, train_set,
               train_label);
    }

    // 2. train the model
    model.train(train_set, train_label, f);

    return model;
  }
};

} // namespace xcache
} // namespace xstore
