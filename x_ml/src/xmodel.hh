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
// TODO: should handle overflow for complex keys
using update_func = std::function<std::pair<int, int>(
    const u64 &label, const u64 &predict, const int &cur_min,
    const int &cur_max)>;

inline std::pair<int, int> default_update_func(const u64 &label,
                                               const u64 &predict,
                                               const int &cur_min,
                                               const int &cur_max) {
  auto new_min = std::min(static_cast<i64>(cur_min),
                          static_cast<i64>(label) - static_cast<i64>(predict));
  auto new_max = std::max(static_cast<i64>(cur_max),
                          static_cast<i64>(label) - static_cast<i64>(predict));
  LOG(0) << "update mm: " << new_min << " " << new_max
         << "; l : "<< label << " " << predict;
  return std::make_pair(new_min,new_max);
}

template <template<typename> class ML,typename Key> struct XSubModel {
  ML<Key> ml;
  /*! right now we use int to record errors, since
    this xsubmodel is only used at the server
  */
  int err_min = 0;
  int err_max = 0;

  int max = 0;

  XSubModel() = default;

  // direct forward the init parameters to the ML
  template <typename... Args> XSubModel(Args... args) : ml(args...) {}

  auto get_point_predict(const Key &k) -> int {
    auto res = this->ml.predict(k);
    res = std::min<int>(res, max);
    return static_cast<int>(std::max<int>(res, static_cast<int>(0)));
  }

  auto total_error() -> int {
    return this->err_max - this->err_min;
  }

  auto get_predict_range(const Key &k) -> std::pair<int,int> {
    auto p = this->get_point_predict(k);
    auto p_start = std::max(0, p + this->err_min);
    auto p_end   = std::min(this->max, p + this->err_max);
    return std::make_pair(p_start, p_end);
  }

  /*!
    Train the ml, and use calculate the min_max according to the train_label,
    update the min-max according to *f*
   */
  auto train(std::vector<Key> &train_data, std::vector<u64> &train_label,
             update_func f = default_update_func) {
    this->train_ml(train_data,train_label);
    this->cal_error(train_data, train_label,f);
  }

  auto train_ml(std::vector<Key> &train_data, std::vector<u64> &train_label) {
    ASSERT(train_data.size() == train_label.size());
    // first train ml
    this->ml.train(train_data, train_label);
    if (!train_label.empty()) {
      this->max = static_cast<int>(train_label[train_label.size() - 1]);
    }
  }

  auto cal_error(std::vector<Key> &train_data, std::vector<u64> &train_label,
                 update_func f = default_update_func) {
    // then calculate the min-max
    for (uint i = 0; i < train_data.size(); ++i) {
      auto k = train_data[i];
      auto label = train_label[i];

      auto res = f(label, static_cast<u64>(this->get_point_predict(k)),
                   this->err_min, this->err_max);
      this->err_min = std::get<0>(res);
      this->err_max = std::get<1>(res);
      // this->max = std::max(this->max, static_cast<int>(train_label[i]));
    }
  }
};

} // namespace xml

} // namespace xstore
