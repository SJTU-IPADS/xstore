#pragma once

#include <string>
#include <vector>

#include "../../deps/r2/src/common.hh"

namespace xstore {

namespace ml {

using namespace r2;

/*!
  An ML trait abstracts a machine learning model,
  which executes `predicts` and `train`.
  It also abstracts away methods for serializing so that we can
  pass this model to different processes (possibly on a different machine).
 */
template <class Derived> class MLTrait {
public:
  double predict(const u64 &key) {
    return reinterpret_cast<Derived *>(this)->predict_impl(key);
  }

  void train(std::vector<u64> &train_data, std::vector<u64> &train_label,
             int step = 1) {
    return reinterpret_cast<Derived *>(this)->train_impl(train_data,
                                                         train_label, step);
  }

  std::string serialize() {
    return reinterpret_cast<Derived *>(this)->serialize_impl();
  }

  /*!
    init myself from `data` returned from serialize.
   */
  void from_serialize(const std::string &data) {
    return reinterpret_cast<Derived *>(this)->from_serialize_impl(data);
  }
};

} // namespace ml

} // namespace xstore
