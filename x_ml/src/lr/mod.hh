#pragma once

#include <mkl.h>
#include <mkl_lapacke.h>

#include "../ml_trait.hh"

#include "../../../xutils/marshal.hh"

namespace xstore {

namespace xml {

/*!
  Core LR Model, leverage MKL for training
 */
template <typename Key>
struct __attribute__((packed)) LR : public MLTrait<LR<Key>, Key> {
  double w;
  double b;

  LR() = default;

  LR(const double &w, const double &b) : w(w), b(b) {}

  auto predict_impl(const Key &key) -> double {
    const auto feature = key.to_feature();
    return feature.at(0) * w + b;
  }

  void train_impl(std::vector<Key> &train_data, std::vector<u64> &train_label,
                  int step = 1) {
    ASSERT(train_data.size() == train_label.size());
    if (train_data.empty()) {
      return;
    }

    ASSERT(train_data.size() > 1) << "train data sz:" << train_data.size();

    auto num_rows = train_data.size();
    auto lr_parameter = 2; // w + b

    std::vector<double> flatted_matrix_A(num_rows * lr_parameter, 0);
    std::vector<double> flatted_matrix_B(num_rows);

    // shrink the training-set if possible

    num_rows = 0;
    for (uint i = 0, j = 0; i < train_data.size(); i += step, j += 1) {
      const auto feature = train_data[i].to_feature();
      flatted_matrix_A[j * lr_parameter] = feature.at(0);
      flatted_matrix_A[j * lr_parameter + 1] = 1;

      flatted_matrix_B[j] = static_cast<double>(train_label[i]);
      num_rows += 1;
    }

    if (num_rows == 1) {
      flatted_matrix_A[num_rows * lr_parameter] =
          train_data[num_rows].to_feature().at(0);
      flatted_matrix_A[num_rows * lr_parameter + 1] = 1;

      flatted_matrix_B[num_rows] = static_cast<double>(train_label[num_rows]);
      num_rows += 1;
    }

    // solve
    auto ret = LAPACKE_dgels(LAPACK_ROW_MAJOR, 'N', num_rows, lr_parameter, 1,
                             &flatted_matrix_A[0], lr_parameter,
                             &flatted_matrix_B[0], 1);

    this->w = static_cast<double>(flatted_matrix_B[0]);
    this->b = static_cast<double>(flatted_matrix_B[1]);

    // done
  }

  auto serialize_impl() -> std::string {
    return ::xstore::util::MarshalT<LR>::serialize(*this);
  }

  void from_serialize_impl(const std::string &data) {
    *this = ::xstore::util::MarshalT<LR>::deserialize(data).value();
  }
};

} // namespace xml
} // namespace xstore
