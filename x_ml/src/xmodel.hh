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
  auto new_min = std::min(static_cast<i64>(cur_min),
                          static_cast<i64>(label) - static_cast<i64>(predict));
  auto new_max = std::max(static_cast<i64>(cur_max),
                          static_cast<i64>(label) - static_cast<i64>(predict));
  LOG(0) << "update mm: " << new_min << " " << new_max
         << "; l : "<< label << " " << predict;
  return std::make_pair(new_min,new_max); // not implemented
}

template <class ML> struct XSubModel {
  ML ml;
  /*! right now we use int to record errors, since
    this xsubmodel is only used at the server
  */
  int err_min = 0;
  int err_max = 0;

  int max = 0;

  XSubModel() = default;

  // direct forward the init parameters to the ML
  template <typename... Args> XSubModel(Args... args) : ml(args...) {}

  auto get_point_predict(const u64 &k) -> int {
    return static_cast<int>(
        std::max(static_cast<int>(this->ml.predict(k)), static_cast<int>(0)));
  }

  auto get_predict_range(const u64 &k) -> std::pair<int,int> {
    auto p = this->get_point_predict(k);
    auto p_start = std::max(0, p + this->err_min);
    auto p_end   = std::min(this->max, p + this->err_max);
    return std::make_pair(p_start, p_end);
  }

  /*!
    Train the ml, and use calculate the min_max according to the train_label,
    update the min-max according to *f*
   */
  void train(std::vector<u64> &train_data, std::vector<u64> &train_label,
             update_func f = default_update_func) {
    ASSERT(train_data.size() == train_label.size());
    // first train ml
    this->ml.train(train_data,train_label);
    this->max = static_cast<int>(train_label.size());

    // then calculate the min-max
    for (uint i = 0;i < train_data.size(); ++i) {
      auto k = train_data[i];
      auto label = train_label[i];

      auto res = f(label, static_cast<u64>(this->get_point_predict(k)),
                   this->err_min, this->err_max);
      this->err_min = std::get<0>(res);
      this->err_max = std::get<1>(res);
    }
  }
};

} // namespace xml

} // namespace xstore
