#pragma once

#include <mkl.h>
#include <mkl_lapacke.h>

#include "./ml_trait.hh"

#include "../../xutils/marshal.hh"

namespace xstore {

namespace xml {

/*!
  Core LR Model, leverage MKL for training
 */
template<usize D, typename Key>
struct __attribute__((packed)) MvLR : public MLTrait<MvLR<D, Key>, Key>
{
  double w_vec[D + 1];
  MvLR() = default;

  explicit MvLR(const float (&w)[D + 1])
  {
    for (uint i = 0; i <= D; ++i)
      w_vec[i] = w[i];
  }

  auto predict_impl(const Key& key) -> double
  {
    const auto feature = key.to_feature();

    double res = 0;
    ASSERT(feature.size() == D)
      << "vec sz mismatch: " << feature.size() << " vs. " << D;
    for (uint i = 0; i < D; ++i) {
      res += w_vec[i] * feature[i];
    }
    res += w_vec[D];
    return res;
  }

  void train_impl(std::vector<Key>& train_data,
                  std::vector<u64>& train_label,
                  int step = 1)
  {
    int m = train_data.size(); // M
    int n = D + 1;             // will be init later

    double* a = (double*)malloc(m * n * sizeof(double));
    double* b = (double*)malloc(std::max(m, n) * sizeof(double));
    std::vector<int> useful_feature_idxs;

    for (uint i = 0; i < D; ++i) {
      useful_feature_idxs.push_back(i);
    }

    bool use_bias = true;

    int fitting_res = 0;
    do {
      n = useful_feature_idxs.size();
      if (use_bias) {
        n += 1;
      }
      for (uint i = 0; i < train_data.size(); ++i) {
        auto feature = train_data[i].to_feature();
        ASSERT(feature.size() == D);
        for (uint j = 0; j < useful_feature_idxs.size(); ++j) {
          a[i * n + j] =
            static_cast<double>(feature.at(useful_feature_idxs[j]));
        }
        if (use_bias) {
          a[i * n + useful_feature_idxs.size()] = 1;
        }
        b[i] = train_label[i];
      }

      for (int b_i = m; b_i < n; b_i++) {
        b[b_i] = 0;
      }

      fitting_res = LAPACKE_dgels(LAPACK_ROW_MAJOR,
                                  'N',
                                  m,
                                  n,
                                  1 /* nrhs */,
                                  a,
                                  n /* lda */,
                                  b,
                                  1 /* ldb, i.e. nrhs */);
      if (fitting_res > 0) {

        if (fitting_res >= useful_feature_idxs.size()) {
          use_bias = false;
        } else {
          size_t feat_i = fitting_res - 1;
          useful_feature_idxs.erase(useful_feature_idxs.begin() + feat_i);
        }

        if (useful_feature_idxs.size() == 0 && use_bias == false) {
          ASSERT(false) << "failed to train mlr!";
        }
      } else if (fitting_res < 0) {
        ASSERT(false);
      }

      // first re-set
      for (uint i = 0; i <= D; ++i) {
        w_vec[i] = 0;
      }

      for (uint i = 0; i < useful_feature_idxs.size(); ++i) {
        ASSERT(useful_feature_idxs[i] < D)
          << useful_feature_idxs[i] << "; sz: " << useful_feature_idxs.size();
        this->w_vec[useful_feature_idxs[i]] = static_cast<double>(b[i]);
      }
      if (use_bias) {
        this->w_vec[D] = static_cast<double>(b[useful_feature_idxs.size()]);
      }
      // this->b = b[n-1];
    } while (fitting_res != 0);
    // LOG(4) << "train done use:" << useful_feature_idxs.size() << " features
    // and bias: " << use_bias;
    free(a);
    free(b);
  }

  auto serialize_impl() -> std::string
  {
    return ::xstore::util::MarshalT<MvLR<D, Key>>::serialize(*this);
  }

  void from_serialize_impl(const ::xstore::string_view& data)
  {
    *this = ::xstore::util::MarshalT<MvLR<D, Key>>::deserialize(data).value();
  }

  void from_file_impl(const ::xstore::string_view& name)
  {
    ASSERT(false) << "not implemented";
  }

  friend std::ostream& operator<<(std::ostream& out, const MvLR<D, Key>& mod)
  {
    for (uint i = 0; i < D; ++i) {
      out << mod.w_vec[i] << " ";
    }
    out << "; bias: " << mod.w_vec[D];
    return out;
  }

  auto serialize_to_file_impl(const std::string& name) -> std::string
  {
    std::ofstream ofs(name + ".mvlr");
    ofs << this->serialize();
    ofs.close();
    return name;
  }

  static auto serialize_sz() -> usize { return sizeof(MvLR<D, Key>); }
};

} // namespace xml
} // namespace xstore
