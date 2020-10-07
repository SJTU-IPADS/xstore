#pragma once

#include <mkl.h>
#include <mkl_lapacke.h>

#include "../marshal.hpp"

namespace fstore {

// Linear regression model
template<typename ParameterType = double>
class LRModel : public ModelTrait
{
public:
  ParameterType w = 0.0;
  ParameterType b = 0.0;

  LRModel() {
  }

  std::string serialize() override
  {
    std::string buf(sizeof(ParameterType) * 2, '\0');
    char* ptr = (char*)(buf.data());
    *((ParameterType*)ptr) = w;
    ptr += sizeof(ParameterType);
    *((ParameterType*)ptr) = b;
    return buf;
  }

  void from_serialize(const std::string &data) override {
    const char *ptr = data.data();
    ASSERT(data.size() >= 2 * sizeof(ParameterType)) << "lr data sz: " << data.size();

    this->w = Marshal<ParameterType>::extract(ptr);
    ptr += sizeof(ParameterType);

    this->b = Marshal<ParameterType>::extract(ptr);

    //LOG(4) << "serialize done";
  }

  bool eq(const LRModel<ParameterType> &m) {
    return w == m.w && b == m.b;
  }

  static usize buf_sz() {
    return sizeof(ParameterType) * 2;
  }

  double
  predict(const u64& key) override
  {
    return static_cast<double>(key) * w + b;
  }

  void train(std::vector<u64>& train_data,
             std::vector<u64>& train_label, int step)
  {
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
    //num_rows = num_rows / step;

    num_rows = 0;
#if 0
    if (num_rows  == 1) {
      // overflow
      flatted_matrix_B.push_back(0);
    }
#endif
    for (uint i = 0, j = 0; i < train_data.size(); i += step, j += 1) {
      flatted_matrix_A[j * lr_parameter] = static_cast<double>(train_data[i]);
      flatted_matrix_A[j * lr_parameter + 1] = 1;

      flatted_matrix_B[j] = static_cast<double>(train_label[i]);
      num_rows += 1;
    }

    if (num_rows == 1) {
      flatted_matrix_A[num_rows * lr_parameter] = static_cast<double>(train_data[num_rows]);
      flatted_matrix_A[num_rows * lr_parameter + 1] = 1;

      flatted_matrix_B[num_rows] = static_cast<double>(train_label[num_rows]);
      num_rows += 1;
    }
    //LOG(4) << "real trained with data: " << num_rows;

    auto ret = LAPACKE_dgels(LAPACK_ROW_MAJOR,
                             'N',
                             num_rows,
                             lr_parameter,
                             1,
                             &flatted_matrix_A[0],
                             lr_parameter,
                             &flatted_matrix_B[0],
                             1);

    //ASSERT(ret == 0) << print_training_set(train_data) << " " << (-ret)
    //<< "-th parameter had an illegal value";

    this->w = static_cast<ParameterType>(flatted_matrix_B[0]);
    this->b = static_cast<ParameterType>(flatted_matrix_B[1]);
  }


};
}
