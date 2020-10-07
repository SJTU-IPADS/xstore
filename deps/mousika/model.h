#include <cassert>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <climits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "mkl.h"
#include "mkl_lapacke.h"

// serialzation / de-serialization
//#include <boost/archive/text_iarchive.hpp>
//#include <boost/archive/text_oarchive.hpp>

#include "marshal.hpp"

#if !defined(COUT_THIS)
#define COUT_THIS(this) std::cout << this << std::endl
#endif  // COUT_THIS

#if !defined(MODEL_H)
#define MODEL_H

#define MKL_MALLOC_ALIGN 64

//typedef unsigned learned_addr_t;
typedef int64_t learned_addr_t;

template <class D>
inline void min_max(const std::vector<D> &vals, D &max, D &min) {
  assert(vals.size() != 0);

  max = vals[0];
  min = vals[0];
  for (const D &val : vals) {
    if (val > max) max = val;
    if (val < min) min = val;
  }
}

template <class D>
inline void mean(const std::vector<D> &vals, D &mean) {
  assert(vals.size() != 0);

  double sum = 0;
  for (const D &val : vals) {
    sum += val;
  }

  mean = sum / vals.size();
}

template <class Model_T>
bool prepare_last_helper(Model_T *model, const std::vector<double> &keys,
                         const std::vector<learned_addr_t> &indexes) {
  double not_used, not_used_either;
  model->prepare(keys, indexes, not_used, not_used_either);

  std::vector<int64_t> errors;
  for (int i = 0; i < keys.size(); ++i) {
    double key = keys[i];
    int64_t index_actual = indexes[i];
    //int64_t index_pred = static_cast<int>(model->predict(key));
    /**
     * According to https://stackoverflow.com/questions/9695329/c-how-to-round-a-double-to-an-int,
     * using std::round is a more stable way to round.
     */
    int64_t index_pred = std::round(model->predict(key));
    errors.push_back(index_actual - index_pred);
    //if(key == 23002729) {
    //      printf("check inserted predict: %d, actual : %d\n",index_pred,index_actual);
    //}
  }

  model->max_error = std::numeric_limits<int64_t>::max();
  model->min_error = std::numeric_limits<int64_t>::min();
  if (errors.size() == 0 && keys.size() == 0) {
    //assert(false);
    //printf("one model has zero error with %lu keys\n",keys.size());
    model->max_error = 0;
    model->min_error = 0;
    return false;
  } else {
    //    for(uint i = 0;i < errors.size();++i)
    //      printf("error of key: %ld\n",errors[i]);
    min_max(errors, model->max_error, model->min_error);
    //printf("after min-max: %ld %ld\n",model->min_error,model->max_error);
  }
  return true;
}

template <class Model_T>
inline void predict_last_helper(Model_T *model, const double key, learned_addr_t &pos,
                         learned_addr_t &error_start, learned_addr_t &error_end) {
  //auto res = model->predict(key);
  //std::cout << "gte pos before cast: " << res << std::endl;;
  //pos = static_cast<int>(model->predict(key));
  //pos = static_cast<int>(res);
  //pos = std::round(res);
  assert(model->max_error >= model->min_error);
  pos = std::round(model->predict(key));
  //std::cout << "gte pos: " << pos << std::endl;;
  error_start = pos + model->min_error;
  error_end = pos + model->max_error + 1;
  if(error_end < 0) {
    printf("min %ld, max: %ld, pos %ld\n",model->min_error,model->max_error,pos);
  }
  //assert(error_end - error_start != 0);
#if 0
  //if(error_start == -1) {
  if(1) {
    fprintf(stdout,"predict %f, pos %d, min %d, max %d\n",key,pos,model->min_error,model->max_error);
  }
#endif
}

class BestMapModel {
 public:
  void prepare(const std::vector<double> &keys,
               const std::vector<learned_addr_t> &indexes, double &index_pred_max, double &index_pred_min) {
    if (keys.size() == 0) return;

    key_size = keys.size();
    assert(keys.size() == indexes.size());
    //printf("index-back:%llu, key_size - 1: %llu\n", indexes.back(),key_size -1);

    for (uint32_t i = 0; i < key_size; ++i) {
      key_index[keys[i]] = indexes[i];
    }

    std::vector<double> index_preds;
    for (const double key: keys) {
      index_preds.push_back(predict(key));
    }
    min_max(index_preds, index_pred_max, index_pred_min);
  }

  double predict(const double key) { return (double)key_index[key]; }

  inline void prepare_last(const std::vector<double> &keys,
                           const std::vector<learned_addr_t> &indexes) {
    prepare_last_helper<BestMapModel>(this, keys, indexes);
  }

  inline void predict_last(const double key, learned_addr_t &pos, learned_addr_t &error_start,
                           learned_addr_t &error_end) {
    predict_last_helper<BestMapModel>(this, key, pos, error_start, error_end);
  }

 public:
  int64_t max_error, min_error;

 private:
  std::map<double, learned_addr_t> key_index;
  uint64_t key_size;
};

#define REPORT_TNUM 1
class LinearRegression {
 public:
  void prepare(const std::vector<double> &keys,
               const std::vector<learned_addr_t> &indexes, double &index_pred_max, double &index_pred_min) {
    std::set<double> unique_keys;
    //printf("model prepare num: %lu\n",keys.size());
    for (double key: keys) {
      unique_keys.insert(key);
    }
#if REPORT_TNUM
    this->num_training_set = unique_keys.size();
#endif

    if (unique_keys.size() == 0) return;

    if (unique_keys.size() == 1) {
      bias = indexes[0];
      w = 0;
      return;
    }

    // if (keys.size() == 0) return;

    // if (keys.size() == 1) {
    //   bias = indexes[0];
    //   w = 0;
    //   return;
    // }

    // use LAPACK to solve least square problem, i.e., to minimize ||b-Ax||_2
    // where b is the actual indexes, A is input keys
    int m = keys.size();  // number of samples
    int n = 2;            // number of features (including the extra 1)
    double *a = (double *)malloc(m * n * sizeof(double));
    double *b = (double *)malloc(m * sizeof(double));
    for (int i = 0; i < m; ++i) {
      a[i * n] = 1;  // the extra 1
      a[i * n + 1] = keys[i];
      b[i] = indexes[i];
    }

    int ret = LAPACKE_dgels(LAPACK_ROW_MAJOR, 'N', m, n, 1 /* nrhs */, a,
                            n /* lda */, b, 1 /* ldb, i.e. nrhs */);

    if (ret > 0) {
      printf("The diagonal element %i of the triangular factor ", ret);
      printf("of A is zero, so that A does not have full rank;\n");
      printf("the least squares solution could not be computed.\n");
      exit(1);
    } else if (ret < 0) {
      printf("%i-th parameter had an illegal value\n", -ret);
    }

    assert(ret == 0);
    bias = b[0];
    w = b[1];

    free(a);
    free(b);

    std::vector<double> index_preds;
    for (const double key: keys) {
      index_preds.push_back(predict(key));
    }
    min_max(index_preds, index_pred_max, index_pred_min);
  }

  double predict(const double key) {
    auto res = bias + w * key;
    //ystd::cout << "predixt:  " << res << "; using key: " << key << std::endl;
    return std::max(res,0.0); // avoid 0 overflow
  }

  inline bool prepare_last(const std::vector<double> &keys,
                           const std::vector<learned_addr_t> &indexes) {
    return prepare_last_helper<LinearRegression>(this, keys, indexes);
  }

  inline void predict_last(const double key, learned_addr_t &pos, learned_addr_t &error_start,
                           learned_addr_t &error_end) {
    predict_last_helper<LinearRegression>(this, key, pos, error_start,
                                          error_end);
  }

 public:
  template<class Archive>
  void serialize(Archive &ar, const unsigned int version) {
    ar & max_error;
    ar & min_error;
    ar & bias;
    ar & w;
  }

  friend std::ostream &operator << (std::ostream &output,
                                    const LinearRegression &lr) {
    output << "LR model, w (" << lr.w << "), bias (" << lr.bias << ");"
           << "error range: [" << lr.min_error << "," << lr.max_error << "]";
    return output;
  }

  bool operator==(const LinearRegression &l) const {
    return
        l.bias == bias &&
        l.w    == w &&
        l.min_error == min_error &&
        l.max_error == max_error;
  }

#if 0
  LinearRegression& operator=(const LinearRegression& other) {
    if (this != &other) {
      bias = other.bias;
      w    = other.w;
      min_error = other.min_error;
      max_error = other.max_error;
    }
    return *this;
  }
#endif
#if 0
  /*!
    Deprected serialization, since it may result in small calcuation error during double transfer.
   */
  static std::string serialize(const LinearRegression &lr) {
    std::ostringstream oss;
    boost::archive::text_oarchive oa(oss);
    oa << lr;
    return oss.str();
  }

  static LinearRegression deserialize(const std::string &s) {
    std::istringstream iss(s);
    boost::archive::text_iarchive ia(iss);

    LinearRegression lr;
    ia >> lr;
    return lr;
  }
#endif

  /*!
   */
  static mousika::Buf_t serialize_hardcore(const LinearRegression &lr) {
    mousika::Buf_t buf;
    mousika::Marshal::serialize_append(buf,lr.w);
    mousika::Marshal::serialize_append(buf,lr.bias);
    mousika::Marshal::serialize_append(buf,lr.min_error);
    mousika::Marshal::serialize_append(buf,lr.max_error);
    return buf;
  }

  static LinearRegression deserialize_hardcore(const mousika::Buf_t &buf) {

    LinearRegression lr;
    bool res = mousika::Marshal::deserialize(buf,lr.w);
    assert(res);
    auto nbuf = mousika::Marshal::forward(buf,0,sizeof(double));
    res = mousika::Marshal::deserialize(nbuf,lr.bias);
    assert(res);
    nbuf = mousika::Marshal::forward(nbuf,0,sizeof(double));
    res = mousika::Marshal::deserialize(nbuf,lr.min_error);
    assert(res);
    nbuf = mousika::Marshal::forward(nbuf,0,sizeof(int64_t));
    res = mousika::Marshal::deserialize(nbuf,lr.max_error);
    assert(res);

    // some sanity checks
    assert(lr.max_error >= lr.min_error);
    assert(static_cast<__int128>(lr.max_error) - static_cast<__int128>(lr.max_error) <= 10240);
    return lr;
  }

  int64_t max_error, min_error;
  double bias, w;
#if REPORT_TNUM
  uint64_t num_training_set;
#endif
};

template <class Weight_T>
class NN {
 public:
  NN(int feat_n, int out_n, int width, int depth, std::string weight_dir)
      : feat_n(feat_n), out_n(out_n), width(width), depth(depth) {
    feat_in =
        (Weight_T *)mkl_malloc(feat_n * sizeof(Weight_T), MKL_MALLOC_ALIGN);
    int next_input_size = feat_n;

    // read hidden layers
    for (int i = 0; i < depth; ++i) {
      std::string kernel_name = get_hidden_name(i + 1, "kernel", weight_dir);
      kernels.push_back(read_weights(next_input_size * width, kernel_name));

      std::string bias_name = get_hidden_name(i + 1, "bias", weight_dir);
      bias.push_back(read_weights(width, bias_name));

      intermediates.push_back(prepare_array(width));

      next_input_size = width;
    }

    // read output layer
    std::string kernel_name = weight_dir + "/out:kernel:0";
    kernels.push_back(read_weights(next_input_size * out_n, kernel_name));

    std::string bias_name = weight_dir + "/out:bias:0";
    bias.push_back(read_weights(out_n, bias_name));

    intermediates.push_back(prepare_array(out_n));
  }

  NN(const NN &from)
      : feat_n(from.feat_n),
        out_n(from.out_n),
        width(from.width),
        depth(from.depth) {
    // re-malloc memories
    feat_in =
        (Weight_T *)mkl_malloc(feat_n * sizeof(Weight_T), MKL_MALLOC_ALIGN);
    int next_input_size = feat_n;

    int i;
    for (i = 0; i < depth; ++i) {
      kernels.push_back(prepare_array(next_input_size * width));
      memcpy(kernels.back(), from.kernels[i],
             next_input_size * width * sizeof(Weight_T));
      bias.push_back(prepare_array(width));
      memcpy(bias.back(), from.bias[i], width * sizeof(Weight_T));
      intermediates.push_back(prepare_array(width));

      next_input_size = width;
    }

    kernels.push_back(prepare_array(next_input_size * out_n));
    memcpy(kernels.back(), from.kernels[i],
           next_input_size * out_n * sizeof(Weight_T));
    bias.push_back(prepare_array(out_n));
    memcpy(bias.back(), from.bias[i], out_n * sizeof(Weight_T));
    intermediates.push_back(prepare_array(out_n));
  }

  ~NN() {
    mkl_free(feat_in);
    for (Weight_T *ptr : kernels) mkl_free(ptr);
    for (Weight_T *ptr : bias) mkl_free(ptr);
    for (Weight_T *ptr : intermediates) mkl_free(ptr);
  }

  void prepare(const std::vector<double> &keys,
               const std::vector<learned_addr_t> &indexes, double &index_pred_max, double &index_pred_min) {
    if (keys.size() == 0) return;

    min_max(keys, key_max, key_min);
    mean(keys, key_mean);
    min_max(indexes, index_max, index_min);
    mean(indexes, index_mean);

    std::vector<double> index_preds;
    for (const double key: keys) {
      index_preds.push_back(predict(key));
    }
    min_max(index_preds, index_pred_max, index_pred_min);
  }

  double predict(const double key) {
    int i = 0;
    const Weight_T key_casted = static_cast<Weight_T>(scale_down_key(key));

    // first hidden layers (scalar * vector + vector)
#ifdef USER_AXPY
    axpy(width, key_casted, kernels[i], bias[i], intermediates[i]);
#else
    std::memcpy(intermediates[i], bias[i], width * sizeof(Weight_T));
    cblas_axpy(width, key_casted, kernels[i], intermediates[i]);
#endif
    relu(intermediates[i], width);

    Weight_T *next_input = intermediates[i];
    int next_input_size = width;
    i++;

    // the rest hidden layers
    for (; i < depth; ++i) {
      std::memcpy(intermediates[i], bias[i], width * sizeof(Weight_T));
      cblas_gemv(next_input_size, width, kernels[i], next_input,
                 intermediates[i]);
      relu(intermediates[i], width);

      // prepare next layer
      next_input = intermediates[i];
      next_input_size = width;
    }

    // output layer

#ifdef USER_DOT
    Weight_T res = dot(next_input_size, next_input, kernels[i]);
#else
    Weight_T res = cblas_dot(next_input_size, next_input, kernels[i]);
#endif

    res += *bias[i];

    return scale_up_index(static_cast<double>(res));
  }

  /*
   * General predict for string input
   */
  double predict(const double *keys) {
    for (int i = 0; i < feat_n; ++i)
      feat_in[i] = static_cast<Weight_T>(keys[i]);
    Weight_T *next_input = feat_in;
    int next_input_size = feat_n;
    int i;

    // hidden layers
    for (i = 0; i < depth; ++i) {
      std::memcpy(intermediates[i], bias[i], width * sizeof(Weight_T));
      cblas_gemv(next_input_size, width, kernels[i], next_input,
                 intermediates[i]);
      relu(intermediates[i], width);

      // prepare next layer
      next_input = intermediates[i];
      next_input_size = width;
    }

    // output layer
    std::memcpy(intermediates[i], bias[i], out_n * sizeof(Weight_T));
    cblas_gemv(next_input_size, out_n, kernels[i], next_input,
               intermediates[i]);

    return static_cast<double>(*intermediates[i]);
  }

  inline void prepare_last(const std::vector<double> &keys,
                           const std::vector<learned_addr_t> &indexes) {
    prepare_last_helper<NN>(this, keys, indexes);
  }

  inline void predict_last(const double key, int &pos, int &error_start,
                           int &error_end) {
    predict_last_helper<NN>(this, key, pos, error_start, error_end);
  }

 private:
  /*
   * Initialization helpers
   */
  std::string get_hidden_name(int i, std::string name,
                              const std::string &parent_dir) {
    std::stringstream ss;
    ss << "hidden-" << i << ":" << name << ":0";
    std::string filename = ss.str();

    if (parent_dir.empty()) {
      return filename;
    }

    if (parent_dir.back() == '/') {
      return parent_dir + filename;
    } else {
      return parent_dir + "/" + filename;
    }
  }

  Weight_T *read_weights(int n,const std::string& value_file) {
    Weight_T *dst = prepare_array(n);
#if 0
    std::fstream infile(value_file);
    assert(infile.is_open());
    for (int i = 0; i < n; ++i) infile >> dst[i];
#endif
    return dst;
  }

  Weight_T *prepare_array(int n) {
    return (Weight_T *)mkl_malloc(n * sizeof(Weight_T), MKL_MALLOC_ALIGN);
  }

  /*
   * Computation helpers
   */
  double scale_down_key(const double key) {
    return (key - key_mean) / (key_max - key_min);
  }

  double scale_up_index(const double index) {
    return index * (index_max - index_min) + index_mean;
  }

  /*
   * gemv impls, which compute y := x*a + y where y and x are vectors and a is a
   * matrix
   */
  void cblas_gemv(int m, int n, float *a, float *x, float *y) {
    cblas_sgemv(CblasRowMajor, CblasTrans, m, n, 1 /* alpha */, a, n, x,
                1 /* incx */, 1 /* beta */, y, 1 /* incy */);
  }

  void cblas_gemv(int m, int n, double *a, double *x, double *y) {
    cblas_dgemv(CblasRowMajor, CblasTrans, m, n, 1 /* alpha */, a, n, x,
                1 /* incx */, 1 /* beta */, y, 1 /* incy */);
  }

  /*
   * axpy impls, which compute y := x*a + y where y and x are vectors and a is a
   * scalar
   */
  void cblas_axpy(int n, float a, float *x, float *y) {
    cblas_saxpy(n, a, x, 1, y, 1);
  }

  void cblas_axpy(int n, double a, double *x, double *y) {
    cblas_daxpy(n, a, x, 1, y, 1);
  }

  void axpy(int n, Weight_T a, Weight_T *x, Weight_T *y, Weight_T *out) {
    for (int i = 0; i < n; ++i) {
      out[i] = x[i] * a + y[i];
    }
  }

  /*
   * dot impls, which compute res := x*y where y and x are vectors and res is a
   * scalar
   */
  float cblas_dot(int n, float *x, float *y) {
    return cblas_sdot(n, x, 1, y, 1);
  }

  double cblas_dot(int n, double *x, double *y) {
    return cblas_ddot(n, x, 1, y, 1);
  }

  Weight_T dot(int n, Weight_T *x, Weight_T *y) {
    Weight_T res = 0;
    for (int i = 0; i < n; ++i) {
      res += x[i] * y[i];
    }
    return res;
  }

  /*
   * relu impl, which compute vals := ReLU(vals) that only retains non-negative
   * values
   */
  void relu(Weight_T *vals, int length) {
    for (int i = 0; i < length; i++) {
      vals[i] = vals[i] > 0 ? vals[i] : 0;
    }
  }

 public:
  int64_t max_error, min_error;

 private:
  const int feat_n, out_n, width, depth;
  double key_min, key_max, key_mean;
  learned_addr_t index_min, index_max, index_mean;

  Weight_T *feat_in;
  std::vector<Weight_T *> kernels;
  std::vector<Weight_T *> bias;
  std::vector<Weight_T *> intermediates;
};

#endif  // MODEL_H
