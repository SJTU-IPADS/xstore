#pragma once

#include "../common.hpp"

#include "../utils/cdf.hpp"

// a public traits for the learned model
namespace fstore {

class ModelTrait
{
public:
  virtual double predict(const u64& key) = 0;
  virtual void train(std::vector<u64>& train_data,
                     std::vector<u64>& train_label, int step = 1) = 0;

  virtual std::string serialize() = 0;

  virtual void from_serialize(const std::string &data) = 0;
};

}

#include "./lr.hh"
#include "./sub_model.hh"
#include "./compact_submodel.hh"

#include "./seralize.hh"

namespace fstore {

template<typename Model0, typename S>
class XCache
{
public:
  Model0 dispatcher;
  std::vector<S> sub_models;

  usize max_addr;

  explicit XCache(int num_sub)
    : sub_models(num_sub)
  {
    ASSERT(num_sub > 0);
    LOG(4) << "init XCache using " << num_sub << " sub models";
  }

  XCache() {
  }

  template<typename T>
  usize force_train_submodels(T& t, char *buf, const usize gap, bool verbose = true, bool dump = false)
  {

    u64 trained_models = 0;
    ::fstore::utils::CDF<usize> error_cdf("");
    ::fstore::utils::CDF<usize> page_cdf("");
    ::fstore::utils::CDF<usize> num_cdf("");

    for (u64 i = 0; i < sub_models.size(); ++i)
    {
      auto &m = sub_models[i];

      //if (m.train_watermark != m.notify_watermark) {
      if (1) {
        m.train(t);
        //ASSERT(m.page_table.size() != 0);
        trained_models += 1;

        if (buf != nullptr) {
          // serialize to the buf
          Serializer::serialize_submodel<S>(m, buf + i * gap); // serialize m -> buf
          //Serializer::serialize_submodel<S>(
          //            m, buf); // serialize m -> buf
        }
      }

      if (verbose && m.page_table.size() != 0) {
        error_cdf.insert(m.max_error - m.min_error);
        page_cdf.insert(m.page_table.size());
        num_cdf.insert(m.num_keys_responsible);
      }
    }

    if (verbose && trained_models > 0) {
      error_cdf.finalize();
      page_cdf.finalize();
      num_cdf.finalize();

      LOG(4) << "average error: " << error_cdf.others.average
             << "; min: " << error_cdf.others.min
             << "max: " << error_cdf.others.max;
      LOG(4) << "cdf: " << error_cdf.dump_as_np_data() << std::endl;

      LOG(4) << "average page entry: " << page_cdf.others.average
             << "; min: " << page_cdf.others.min
             << "max: " << page_cdf.others.max;
      LOG(4) << "cdf: " << page_cdf.dump_as_np_data() << std::endl;

      LOG(4) << "average num: " << num_cdf.others.average
             << "; min: " << num_cdf.others.min
             << "max: " << num_cdf.others.max;
      LOG(4) << "cdf: " << num_cdf.dump_as_np_data() << std::endl;
      ASSERT(false);

      // done
    }
    return trained_models;
  }

  template<typename T>
  usize train_submodels(T& t, char *buf, const usize gap, int step, bool verbose = true, bool dump_res = false)
  {

    u64 trained_models = 0;
    ::fstore::utils::CDF<usize> error_cdf("");
    ::fstore::utils::CDF<usize> page_cdf("");
    ::fstore::utils::CDF<usize> sz_cdf("");
    ::fstore::utils::CDF<usize> num_cdf("");
    ::fstore::utils::CDF<double> time_cdf("");

    usize dumped = 0;
    for (u64 i = 0; i < sub_models.size(); ++i)
    {
      auto &m = sub_models[i];

      if (m.train_watermark != m.notify_watermark) {
        // if (1) {
        Option<usize> idx = {};
        if (dump_res && dumped < 10) {
          idx = i;
        }

        r2::Timer tt;
        if (m.train(t,step,idx)) {
          dumped += 1;
        }
        r2::compile_fence();

        if (verbose) {
          time_cdf.insert(tt.passed_msec());
        }

        ASSERT(m.page_table.size() != 0);
        trained_models += 1;
#if 0
        if (buf != nullptr) {
          // serialize to the buf
          Serializer::serialize_submodel<S>(m,
                                            buf +
                                              i * gap); // serialize m -> buf
          // Serializer::serialize_submodel<S>(
          //            m, buf); // serialize m -> buf
        }
#endif
      }

      if (verbose && m.page_table.size() > 0) {
        error_cdf.insert(m.max_error - m.min_error);
        page_cdf.insert(m.page_table.size());
        sz_cdf.insert(m.total_sz());

        num_cdf.insert(m.num_keys_responsible);
      }
    }

    if (verbose && trained_models > 0) {
      error_cdf.finalize();
      page_cdf.finalize();
      sz_cdf.finalize();
      num_cdf.finalize();
      time_cdf.finalize();

      LOG(4) << "average error: " << error_cdf.others.average
             << "; min: " << error_cdf.others.min
             << "max: " << error_cdf.others.max;
      LOG(4) << "cdf: " << error_cdf.dump_as_np_data() << std::endl;

      LOG(4) << "average page entry: " << page_cdf.others.average
             << "; min: " << page_cdf.others.min
             << "max: " << page_cdf.others.max;
      LOG(4) << "cdf: " << page_cdf.dump_as_np_data() << std::endl;
      LOG(4) << "average sz: " << sz_cdf.others.average;

      LOG(4) << "average num: " << num_cdf.others.average
             << "; min: " << num_cdf.others.min
             << "max: " << num_cdf.others.max;
      LOG(4) << "cdf: " << num_cdf.dump_as_np_data() << std::endl;

      LOG(4) << "average time: " << time_cdf.others.average
             << "; min: " << time_cdf.others.min
             << "max: " << time_cdf.others.max;
      LOG(4) << "cdf: " << time_cdf.dump_as_np_data() << std::endl;
      // done
    }
    return trained_models;
  }

  S& get_model(const u64& key) {
    return sub_models[this->select_submodel(key)];
  }

  // return: average key assigned to each model
  template<typename TreeIter>
  usize train_dispatcher(TreeIter& it, bool dump_keys = false)
  {
    std::vector<u64> data;
    std::vector<u64> label;

    ::fstore::utils::DataMap<u64, u64> key_space("keys");

    usize counter = 0;

    for (it.begin(); it.valid(); it.next()) {
      if (it.key() == 3541629798) {
        ASSERT(false) << 3541629798 << " exists!";
      }

      data.push_back(it.key());

      if (dump_keys) {
        key_space.insert(it.key(), counter);
      }

      label.push_back(counter++);
    }

    if (dump_keys) {
      FILE_WRITE("keys.py", std::ofstream::out) << key_space.dump_as_np_data();
    }

    r2::Timer t;
    ASSERT(data.size() > 1);
    dispatcher.train(data, label, 2);
    r2::compile_fence();
    LOG(4) << "train dispatcher in : " << t.passed_msec() << " msec" << " using " << counter << " keys";
    max_addr = counter;
    //LOG(4) << "dispatched done";

    ::fstore::utils::CDF<usize> num_cdf("");
    it.begin();

    // now dispatch the keys

    // assume: keys are sorted
    int count = 0;
    while (1) {
    start:
      ASSERT(it.valid());
      auto cur_key = it.key();
      auto model = select_submodel(cur_key);

      it.next();
      u64 end_key = cur_key;
      count += 1;

      while (it.valid()) {
        auto select = select_submodel(it.key());
        if (it.key() == 3541629798) {
          LOG(4) << "sanityh check select: " << select; ASSERT(false);
        }
        if (select != model) {
          // now we assign the key
          sub_models[model].lock.lock();
          sub_models[model].reset_keys(cur_key, end_key);
          sub_models[model].notify_watermark += 1;
          sub_models[model].lock.unlock();

          // LOG(4) << "assign keys from [" << cur_key << ":" << end_key << "]"
          // << " to model #" << model << " with " << count << " elems";
          num_cdf.insert(count);
          count = 0;
          goto start;
        }
        count += 1;
        end_key = it.key();
        it.next();
      }

      // clean up
      sub_models[model].lock.lock();
      sub_models[model].reset_keys(cur_key, end_key);
      sub_models[model].notify_watermark += 1;
      sub_models[model].lock.unlock();

      // LOG(4) << "assign keys from [" << cur_key << ":" << end_key << "]"
      //<< " to model #" << model << " with " << count << "elems";
      break;
    }

    num_cdf.finalize();
    LOG(4) << "average model responsible num: " << num_cdf.others.average << "; min: " << num_cdf.others.min
           << "max: " << num_cdf.others.max;
    LOG(4) << "model responsible um cdf: " << num_cdf.dump_as_np_data();
    // done
  }

  usize select_submodel(const u64& key)
  {
    auto predict = static_cast<int>(dispatcher.predict(key));
    usize ret = 0;
    if (predict < 0) {
      ret = 0;
    }
    else if (predict >= max_addr) {
      ret = sub_models.size() - 1;
    } else {
      ret = static_cast<usize>((static_cast<double>(predict) / max_addr) *
                               sub_models.size());
    }
    ASSERT(ret < sub_models.size())
      << "predict: " << predict << " " << sub_models.size();
    // LOG(4) << "select " << ret << " for " << key << " with predict: " <<
    // predict;
    return ret;
  }
};

}

#include "./augmentor.hh"
