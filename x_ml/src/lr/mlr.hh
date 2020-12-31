#pragma once

#include "./compact.hh"
#include "../../../xutils/marshal.hh"

namespace xstore {

namespace xml {

/*!
  Unlike LR, MLR(multi-variate LR) would predict a value from [0:base)
  BT:  type of the area; usually is usize;
  LRT: a base LR model for prediction;
  Key: the key used to predict.
 */
// BT states for BaseType
template <typename BT, template<typename> class LRT, typename Key>
struct __attribute__((packed)) MLR : public MLTrait<MLR<BT,LRT,Key>, Key> {
  using Self = MLR<BT,LRT,Key>;
  LRT<Key>       lr;
  BT             base;

  void set_base(const BT &b) {
    this->base = b;
  }

  auto predict_impl(const Key &key) -> double {
    auto res = this->lr.predict(key);
    if (res < 0) {
      res = 0;
    } else if (res > base) {
      res = base;
    }
    return res;
  }

  void train_impl(std::vector<Key> &train_data, std::vector<u64> &train_label,
                  int step) {
    this->lr.train(train_data, train_label,step);
  }

  auto serialize_impl() -> std::string {
    return ::xstore::util::MarshalT<Self>::serialize(*this);
  }

  void from_serialize_impl(const std::string_view &data) {
    *this = ::xstore::util::MarshalT<Self>::deserialize(data).value();
  }
};

}
} // namespace xstore
