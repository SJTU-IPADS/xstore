#pragma once

#include <functional>
#include <utility>

#include "./ml_trait.hh"

namespace xstore {

namespace xml {

/*!
  The ML must implement the ml_trait

  The XSubmodel provides two err bounds (err_min, err_max),
  such that it predict a range instead of some single num,
  i.e., x->predict(key) -> [s,e]; such that the label corresponds
  to the key (l) must be in [s,e].

  User must provides an updating function, during training to
  let XSubmodel defines how to calculate the error.
 */
using update_func = std::function<std::pair<int, int>(
    const u64 &label, const u64 &predict, const int &cur_min,
    const int &cur_max)>;

inline std::pair<int, int> default_update_func(const u64 &label,
                                               const u64 &predict,
                                               const int &cur_min,
                                               const int &cur_max) {
  return std::make_pair(0,0); // not implemented
}

template <class ML> struct XSubModel {
  ML ml;
  /*! right now we use int to record errors, since
    this xsubmodel is only used at the server
  */
  int err_min = 0;
  int err_max = 0;

  u64 max = 0;

  XSubModel() = default;

  /*!
    Train the ml, and use calculate the min_max according to the train_label,
    update the min-max according to *f*
   */
  void train(std::vector<u64> &train_data, std::vector<u64> &train_label,
             update_func f = default_update_func) {
    // TODO
  }
};

} // namespace xml

} // namespace xstore
