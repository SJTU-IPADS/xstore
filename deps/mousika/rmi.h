#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <set>
#include <utility>

#include "mkl_lapacke.h"

#include "model.h"

#define LRfirst
#define AUG

#if !defined(COUT_THIS)
#define COUT_THIS(this) std::cerr << this << std::endl
#endif // COUT_THIS

#if !defined(RMI_H)
#define RMI_H

#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

template <class Weight_T>
class MixTopStage
{
public:
  MixTopStage(int feat_n,
              int out_n,
              int width,
              int depth,
              std::string weight_dir,
              std::vector<std::pair<double, double>> nn_ranges)
      : nn(NN<Weight_T>(feat_n, out_n, width, depth, weight_dir)), nn_ranges(nn_ranges)
  {
    // sanity chekc the nn_ranges
    struct myclass
    {
      bool operator()(std::pair<double, double> i, std::pair<double, double> j)
      {
        return (i.first < j.first) ||
               (i.first == j.first && i.second < j.second);
      }
    } my_comparitor;

    sort(nn_ranges.begin(), nn_ranges.end(), my_comparitor);

    for (int i = 0; i < nn_ranges.size(); ++i)
    {
      assert(nn_ranges[i].first <= nn_ranges[i].second);
    }
    for (int i = 0; i < nn_ranges.size() - 1; ++i)
    {
      assert(nn_ranges[i].second <= nn_ranges[i + 1].first);
    }

    models.emplace_back();
    models.back().is_lr = true;
    models.back().range_end = std::numeric_limits<double>::max();
    lr_data_in.emplace_back();
    for (auto &range : nn_ranges)
    {
      models.back().range_end = range.first;
      models.emplace_back();
      lr_data_in.emplace_back();
      models.back().is_lr = false;
      models.back().range_end = range.second;
      models.emplace_back();
      lr_data_in.emplace_back();
      models.back().is_lr = true;
      models.back().range_end = std::numeric_limits<double>::max();
    }
  }

  inline void prepare(const std::vector<double> &keys,
                      const std::vector<learned_addr_t> &indexes,
                      unsigned model_i,
                      double &index_pred_max,
                      double &index_pred_min)
  {
    nn.prepare(keys, indexes, index_pred_max, index_pred_min);

    for (int i = 0; i < keys.size(); ++i)
    {
      for (int model_i = 0; model_i < models.size(); ++model_i)
      {
        auto &model = models[model_i];
        if (model.range_end < keys[i])
          continue;

        if (model.is_lr)
        {
          lr_data_in[model_i].first.push_back(keys[i]);
          lr_data_in[model_i].second.push_back(indexes[i]);
        }

        break;
      }
    }
    for (int model_i = 0; model_i < models.size(); ++model_i)
    {
      auto &model = models[model_i];
      if (model.is_lr)
      {
        double lr_index_pred_max, lr_index_pred_min;
        model.lr.prepare(lr_data_in[model_i].first,
                         lr_data_in[model_i].second,
                         lr_index_pred_max,
                         lr_index_pred_min);

        if (lr_index_pred_max > index_pred_max)
          index_pred_max = lr_index_pred_max;

        if (lr_index_pred_min < index_pred_min)
          index_pred_min = lr_index_pred_min;
      }
    }
  }
  // inline void prepare_last(const std::vector<double> &keys,
  //                          const std::vector<unsigned> &indexes,
  //                          unsigned model_i) {
  //   models[model_i].prepare_last(keys, indexes);
  // }

  inline double predict(const double key, unsigned model_i)
  {
    for (auto &model : models)
    {
      if (model.range_end < key)
        continue;

      if (model.is_lr)
        return model.lr.predict(key);
      else
        return nn.predict(key);
    }
  }

  // inline void predict_last(const double key, int &pos, int &error_start,
  //                          int &error_end, unsigned model_i) {
  //   models[model_i].predict_last(key, pos, error_start, error_end);
  // }

  inline unsigned get_model_n() const { return 1; }

  // inline int get_max_error(unsigned model_i) const {
  //   return models[model_i].max_error;
  // }

  // inline int get_min_error(unsigned model_i) const {
  //   return models[model_i].min_error;
  // }

  void reset_data()
  {
    data_in =
        std::vector<std::pair<std::vector<double>, std::vector<learned_addr_t>>>(
            get_model_n());
  }

  void assign_data(const double key,
                   const unsigned index,
                   const unsigned model_i)
  {
    data_in[model_i].first.push_back(key);
    data_in[model_i].second.push_back(index);
  }

  std::vector<std::pair<std::vector<double>, std::vector<learned_addr_t>>>
      data_in; // valid during preparing stages
private:
  struct SegModel
  {
    double range_end;
    bool is_lr;
    LinearRegression lr;
  };

  NN<Weight_T> nn;
  std::vector<SegModel> models;
  std::vector<std::pair<double, double>> nn_ranges;
  std::vector<std::pair<std::vector<double>, std::vector<learned_addr_t>>>
      lr_data_in;
};
class BestStage
{
public:
  BestStage(unsigned model_n)
  {
    for (int model_i = 0; model_i < model_n; ++model_i)
    {
      models.emplace_back();
    }
  }

  inline void prepare(const std::vector<double> &keys,
                      const std::vector<learned_addr_t> &indexes,
                      unsigned model_i,
                      double &index_pred_max,
                      double &index_pred_min)
  {
    models[model_i].prepare(keys, indexes, index_pred_max, index_pred_min);
  }

  inline void prepare_last(const std::vector<double> &keys,
                           const std::vector<learned_addr_t> &indexes,
                           unsigned model_i)
  {
    models[model_i].prepare_last(keys, indexes);
  }

  inline double predict(const double key, unsigned model_i)
  {
    return models[model_i].predict(key);
  }

  inline void predict_last(const double key,
                           learned_addr_t &pos,
                           learned_addr_t &error_start,
                           learned_addr_t &error_end,
                           unsigned model_i)
  {
    models[model_i].predict_last(key, pos, error_start, error_end);
  }

  inline unsigned get_model_n() const { return models.size(); }

  inline int get_max_error(unsigned model_i) const
  {
    return models[model_i].max_error;
  }

  inline int get_min_error(unsigned model_i) const
  {
    return models[model_i].min_error;
  }

  void reset_data()
  {
    data_in =
        std::vector<std::pair<std::vector<double>, std::vector<learned_addr_t>>>(
            get_model_n());
  }

  void assign_data(const double key,
                   const unsigned index,
                   const unsigned model_i)
  {
    data_in[model_i].first.push_back(key);
    data_in[model_i].second.push_back(index);
  }

  std::vector<std::pair<std::vector<double>, std::vector<learned_addr_t>>>
      data_in; // valid during preparing stages
private:
  std::vector<BestMapModel> models;
};

class LRStage
{
public:
  LRStage(unsigned model_n)
  {
    for (int model_i = 0; model_i < model_n; ++model_i)
    {
      models.emplace_back();
    }
  }

  LRStage(const std::vector<std::string> &stages)
  {
    for (const auto &s : stages)
    {
      models.push_back(LinearRegression::deserialize_hardcore(s));
    }
  }

  inline void prepare(const std::vector<double> &keys,
                      const std::vector<learned_addr_t> &indexes,
                      unsigned model_i,
                      double &index_pred_max,
                      double &index_pred_min)
  {
    models[model_i].prepare(keys, indexes, index_pred_max, index_pred_min);
  }

  inline void prepare_last(const std::vector<double> &keys,
                           const std::vector<learned_addr_t> &indexes,
                           unsigned model_i)
  {
    if(!models[model_i].prepare_last(keys, indexes)) {
      //printf("[!!!!] model %u has 0 key\n",model_i);
    }
  }

  inline double predict(const double key, unsigned model_i)
  {
    auto res = models[model_i].predict(key);
    // printf("model_i: %u, predict: %f to %f\n",model_i,key,res);
    return res;
  }

  inline void predict_last(const double key,
                           learned_addr_t &pos,
                           learned_addr_t &error_start,
                           learned_addr_t &error_end,
                           unsigned model_i)
  {
    models[model_i].predict_last(key, pos, error_start, error_end);
  }

  inline unsigned get_model_n() const { return models.size(); }

  inline int get_max_error(unsigned model_i) const
  {
    return models[model_i].max_error;
  }

  inline int get_min_error(unsigned model_i) const
  {
    return models[model_i].min_error;
  }

  void reset_data()
  {
    data_in =
        std::vector<std::pair<std::vector<double>, std::vector<learned_addr_t>>>(
            get_model_n());
  }

  void assign_data(const double key,
                   const unsigned index,
                   const unsigned model_i)
  {
    data_in[model_i].first.push_back(key);
    data_in[model_i].second.push_back(index);
  }

  std::vector<std::pair<std::vector<double>, std::vector<learned_addr_t>>>
      data_in; // valid during preparing stages
  // private:
  std::vector<LinearRegression> models;
};

template <class Weight_T>
class NNStage
{
public:
  NNStage(int feat_n,
          int out_n,
          int width,
          int depth,
          std::string weight_dir,
          unsigned model_n)
  {
    for (int model_i = 0; model_i < model_n; ++model_i)
    {
      this->models.emplace_back(feat_n, out_n, width, depth, weight_dir);
    }
  }

  inline void prepare(const std::vector<double> &keys,
                      const std::vector<learned_addr_t> &indexes,
                      unsigned model_i,
                      double &index_pred_max,
                      double &index_pred_min)
  {
    models[model_i].prepare(keys, indexes, index_pred_max, index_pred_min);
  }

  inline void prepare_last(const std::vector<double> &keys,
                           const std::vector<learned_addr_t> &indexes,
                           unsigned model_i)
  {
    models[model_i].prepare_last(keys, indexes);
  }

  inline double predict(const double key, unsigned model_i)
  {
    return models[model_i].predict(key);
  }

  inline void predict_last(const double key,
                           int &pos,
                           int &error_start,
                           int &error_end,
                           unsigned model_i)
  {
    models[model_i].predict_last(key, pos, error_start, error_end);
  }

  inline unsigned get_model_n() const { return models.size(); }

  inline int get_max_error(unsigned model_i) const
  {
    return models[model_i].max_error;
  }

  inline int get_min_error(unsigned model_i) const
  {
    return models[model_i].min_error;
  }

  void reset_data()
  {
    data_in =
        std::vector<std::pair<std::vector<double>, std::vector<learned_addr_t>>>(
            get_model_n());
  }

  void assign_data(const double key,
                   const unsigned index,
                   const unsigned model_i)
  {
    data_in[model_i].first.push_back(key);
    data_in[model_i].second.push_back(index);
  }

  std::vector<std::pair<std::vector<double>, std::vector<learned_addr_t>>>
      data_in; // valid during preparing stages

private:
  std::vector<NN<Weight_T>> models;
};

struct RMIConfig
{
  struct StageConfig
  {
    unsigned model_n;
    enum model_t
    {
      LinearRegression,
      NeuralNetwork,
      BestMapModel,
      MixTopModel,
      Unknown
    } model_type;
    struct
    {
      int depth, width;
      std::string weight_dir;
    } nn_config;
    struct
    {
      int depth, width;
      std::string weight_dir;
      std::vector<std::pair<double, double>> nn_ranges;
    } mix_top_config;
#ifdef SCALE_HOT_PART
    std::vector<std::pair<double, double>> hot_parts;
#endif
  };

  std::vector<StageConfig> stage_configs;

  friend std::ostream &operator<<(std::ostream &output, const RMIConfig &p)
  {
    output << "RMI uses " << p.stage_configs.size() << " stages." << std::endl;
    for (uint i = 0; i < p.stage_configs.size(); ++i)
    {
      output << "stage #" << i
             << " uses parameter: " << p.stage_configs[i].model_n << std::endl;
    }
    return output;
  }
};

template <class Weight_T>
class RMINew
{
public:
  RMINew(const RMIConfig &config)
      : config(config)
  {
    assert(config.stage_configs.size() == 2);
    assert(config.stage_configs.front().model_n == 1);
    assert(config.stage_configs[1].model_type ==
           RMIConfig::StageConfig::LinearRegression);
    printf("rmi init with model num: %u\n",config.stage_configs[1].model_n);
#ifdef EVENLY_ASSIGN
    COUT_THIS("RMI use new-dispatch!");
#else
    COUT_THIS("RMI use original-dispatch!");
#endif
    // init models stage by stage
#ifdef LRfirst
    first_stage = new LRStage(config.stage_configs[0].model_n);
#elif defined(BestModel)
    first_stage = new BestStage(config.stage_configs[0].model_n);
#else
    first_stage =
        new NNStage<Weight_T>(1,
                              1,
                              config.stage_configs[0].nn_config.width,
                              config.stage_configs[0].nn_config.depth,
                              config.stage_configs[0].nn_config.weight_dir,
                              config.stage_configs[0].model_n);
#endif
    second_stage = new LRStage(config.stage_configs[1].model_n);
  }

  RMINew(const std::vector<std::string> &first,
         const std::vector<std::string> &second)
  {
    /**
     * XD: current RMINew only supports LR serialization.
     */
    assert(first.size() == 1);
    first_stage = new LRStage(first);
    second_stage = new LRStage(second);
  }

  RMINew(const std::vector<std::string> &first,const RMIConfig &config) {
    first_stage = new LRStage(first);
    printf("second stage num %d\n",config.stage_configs[1].model_n);
    second_stage = new LRStage(config.stage_configs[1].model_n);
  }

  ~RMINew()
  {
    delete first_stage;
    delete second_stage;
  }

  RMINew(const RMINew &) = delete;
  RMINew(RMINew &) = delete;

  void insert(const double key) { all_keys.push_back(key); }

  void insert_w_idx(const double key, learned_addr_t addr)
  {
    all_keys.push_back(key);
    all_addrs.push_back(addr);
  }

  inline double get_key(uint64_t i)
  {
    return first_stage->data_in.front().first[i];
  }

  inline uint32_t get_index(uint64_t i)
  {
    return first_stage->data_in.front().second[i];
  }

  void finish_insert(bool train_first_layer = true)
  {
    if (all_keys.empty())
      return;

    key_n = all_keys.size();

    sort(all_keys.begin(), all_keys.end());
    printf("finish insert with: %u keys\n", all_keys.size());

    // extract <key,index> of unique keys
    double prev_unique_key = all_keys.front();
    // double prev_unique_index = 0;
    std::vector<double> unique_keys{all_keys.front()};
    std::vector<learned_addr_t> all_indexes;
    std::vector<learned_addr_t> unique_indexes{0};

    /**
     * XD: todo refine: these codes are ugly, and takes extra memory
     */
    if (all_addrs.size() == all_keys.size())
    {
      // scale = true;
      // max_addr = *std::max_element(all_addrs.begin(),all_addrs.end());
      key_n = *std::max_element(all_addrs.begin(), all_addrs.end());
      // printf("learned index uses inserted addrs\n");
      /**
       * XD: my current extension assumes the upper layer will ensure
       * that the keys are all unique.
       */
      // unique_keys.push_back(prev_unique_key);
      // unique_indexes.push_back(all_addrs.front());

      for (uint i = 0; i < all_keys.size(); ++i)
      {
        double this_key = all_keys[i];
        if (this_key != prev_unique_key)
        {
          prev_unique_key = this_key;
          unique_keys.push_back(this_key);
          unique_indexes.push_back(all_addrs[i]);
        }
        // printf("push addr %u, all %u\n",all_addrs[i],all_keys.size());

        // unique_keys.push_back(all_keys[i]);
        // unique_indexes.push_back(all_addrs[i]);
        all_indexes.push_back(all_addrs[i]);
      }
    }
    else
    {
      // unique_keys.push_back(prev_unique_key);
      // unique_indexes.push_back(0);
      for (int index = 0; index < all_keys.size(); ++index)
      {
        double this_key = all_keys[index];
        if (this_key != prev_unique_key)
        {
          prev_unique_key = this_key;
          unique_keys.push_back(this_key);
          unique_indexes.push_back(index);
          // prev_unique_index = index;
        }
        else
        {
          // printf("get non unique key: %f\n",this_key);
        }
        // printf("push addr of normal  %u, total %u\n",index,all_keys.size());
        all_indexes.push_back(index);
      }
    }

    COUT_THIS("number of unique key to RMI: " << unique_keys.size());

    // feed all data to the only model in the 1st stage
    first_stage->reset_data();
    second_stage->reset_data();

    //assert(unique_keys.size() == all_keys.size());
    assert(unique_keys.size() == unique_indexes.size());

    //if (train_first_layer) {
    if (1) {
      for (int i = 0; i < unique_keys.size(); ++i) {
        first_stage->data_in.front().first.push_back(unique_keys[i]);
        first_stage->data_in.front().second.push_back(unique_indexes[i]);
      }
    }

    assert(first_stage->get_model_n() == 1);
    // prepare 1st stage model with fed in data
    for (int model_i = 0; model_i < first_stage->get_model_n(); ++model_i)
    {
      std::vector<double> &uni_keys = first_stage->data_in[model_i].first;
      std::vector<learned_addr_t> &uni_indexes =
          first_stage->data_in[model_i].second;

      // else, dispatch data to the next stage after preparing
      COUT_THIS("normal-first_stage");
      if (train_first_layer) {
        first_stage->prepare(uni_keys,
                             uni_indexes,
                             model_i,
                             first_stage_pred_max,
                             first_stage_pred_min);
      } else {
        //assert(false);
        assert(uni_keys.size() != 0);
      }


#ifdef NORMAL_SECOND_STAGE
      printf("[RMI] first stage trained done, start training the second stage");
      for (int i = 0; i < uni_keys.size(); ++i)
      {
        double index_pred = first_stage->predict(uni_keys[i], model_i);
        unsigned next_stage_model_i = pick_next_stage_model(index_pred);
        second_stage->assign_data(
            uni_keys[i], uni_indexes[i], next_stage_model_i);
#ifdef AUG
        if (i + 1 < uni_keys.size()) {
          // check next key
          double index_pred = first_stage->predict(uni_keys[i + 1],0);
          unsigned next_stage_model_i_1 = pick_next_stage_model(index_pred);
          if(next_stage_model_i_1 != next_stage_model_i) {
            // augument the data
            second_stage->assign_data(
                                      uni_keys[i + 1], uni_indexes[i + 1], next_stage_model_i);
          }
        }
#endif
      }
      // actually we need to augument 0 sized models
#else
      printf("[RMI] first stage trained done, start training the second stage\n");
      int aug_keys = 0;
      for (int i = 0; i < all_keys.size(); ++i)
      {
        double index_pred = first_stage->predict(all_keys[i], model_i);
        unsigned next_stage_model_i = pick_next_stage_model(index_pred);
#ifndef AUF
        second_stage->assign_data(
            all_keys[i], all_indexes[i], next_stage_model_i);
#endif
        // real code here
        if(i >= uni_keys.size()) {
          //printf("uni keys sz: %u, all keys sz %u\n",uni_keys.size(), all_keys.size());
          //assert(false);
        } else {
          this->addrs_map.insert(std::make_pair(uni_keys[i], uni_indexes[i]));
        }

#ifdef AUG

        // my previous key
        if(i - 1 > 0) {
          // augument the previous model
          double index_pred = first_stage->predict(uni_keys[i - 1],0);
          unsigned next_stage_model_i_1 = pick_next_stage_model(index_pred);

          if(next_stage_model_i_1 != next_stage_model_i) {
            // augument the data
            second_stage->assign_data(
                                      uni_keys[i - 1], uni_indexes[i - 1], next_stage_model_i);
            aug_keys += 1;
          }
          // my key
          second_stage->assign_data(
                                    all_keys[i], all_indexes[i], next_stage_model_i);
          // my next key
          if (i + 1 < uni_keys.size()) {
            // check next key
            double index_pred = first_stage->predict(uni_keys[i + 1],0);
            unsigned next_stage_model_i_1 = pick_next_stage_model(index_pred);
            if(next_stage_model_i_1 != next_stage_model_i) {
              // augument the data
              second_stage->assign_data(
                                        uni_keys[i + 1], uni_indexes[i + 1], next_stage_model_i);
              aug_keys += 1;
            }
          }
        }
#endif
      }
      printf("[RMI] total %d keys augumented",aug_keys);
#endif
    }
  }

  /*!
    Add a specific key to a specific model
  */
  void augment_model(const double &key, unsigned model_id)
  {
    assert(addrs_map.find(key) != addrs_map.end());
    second_stage->assign_data(key, addrs_map[key], model_id);
  }

  void finish_train()
  {
    if (all_keys.empty())
      return;
    // prepare 2st stage model with fed in data
    for (int model_i = 0; model_i < second_stage->get_model_n(); ++model_i)
    {
      //printf("train second stage: %d\n", model_i);
      std::vector<double> &keys = second_stage->data_in[model_i].first;
      std::vector<learned_addr_t> &indexes =
          second_stage->data_in[model_i].second;
      if(keys.size() == 0) {
        //printf("model: %d has 0 training data.\n",model_i);
      }

      // let it track the errors itself
      second_stage->prepare_last(keys, indexes, model_i);
    }
    printf("second stage done\n");
    if (first_stage->data_in.size() > 0) {
      first_stage->data_in.clear();
    }
    printf("clear first stage done\n");
    second_stage->data_in.clear();
    all_keys.clear();
    addrs_map.clear();
  }

  void predict_pos_w_model(const double &key,
                           const unsigned &model,
                           learned_addr_t &pos,
                           learned_addr_t &error_start,
                           learned_addr_t &error_end)
  {
    if (unlikely(model >= second_stage->models.size()))
    {
      error_start = std::numeric_limits<learned_addr_t>::max();
      error_end = std::numeric_limits<learned_addr_t>::min();
      return;
    }
    second_stage->predict_last(key, pos, error_start, error_end, model);
    error_end = std::min(error_end, static_cast<int64_t>(key_n));
    error_start = std::max(error_start, static_cast<int64_t>(0));
  }

  void predict_pos(const double key,
                   learned_addr_t &pos,
                   learned_addr_t &error_start,
                   learned_addr_t &error_end)
  {
    double index_pred = first_stage->predict(key, 0);
    unsigned next_stage_model_i = pick_next_stage_model(index_pred);
    second_stage->predict_last(
        key, pos, error_start, error_end, next_stage_model_i);
  }

// private:
#ifdef SCALE_HOT_PART
  uint32_t scale_factor = 80;
  std::vector<std::pair<double, double>> hot_parts;
  double hot_part_left_key, hot_part_right_key;
  int64_t hot_part_left_index = -10, hot_part_right_index = -10;
  uint32_t hot_part_key_n;
  uint32_t scaled_key_n;
  uint32_t scaled_right_index;

  inline unsigned pick_next_stage_model(double index_pred)
  {
    unsigned next_stage_model_n = second_stage->get_model_n();
    unsigned next_stage_model_i;

    if (index_pred >= key_n)
    {
      next_stage_model_i = next_stage_model_n - 1;
    }
    else if (index_pred < 0)
    {
      next_stage_model_i = 0;
    }
    else
    {
      double index;
      if (index_pred < hot_part_left_index)
      {
        index = index_pred;
      }
      else if (index_pred <= hot_part_right_index)
      {
        index = (index_pred - hot_part_left_index) * scale_factor +
                hot_part_left_index;
      }
      else
      {
        // printf("right->%u, %f\n", scaled_right_index, index_pred);
        index = scaled_right_index +
                (index_pred - hot_part_left_index - hot_part_key_n);
      }
      if (scale)
      {
        assert(false);
        next_stage_mode_i =
            static_cast<unsigned>(index / max_addr * next_stage_model_n);
      }
      else
      {
        assert(false);
        next_stage_model_i =
            static_cast<unsigned>(index / scaled_key_n * next_stage_model_n);
      }
      // printf("%f, %u\n", index, next_stage_model_i);
    }
    return next_stage_model_i;
  }
#else
public:
  inline unsigned pick_model_for_key(double key)
  {
    double index_pred = first_stage->predict(key, 0);
    return pick_next_stage_model(index_pred);
  }

  inline unsigned pick_next_stage_model(double index_pred)
  {
    unsigned next_stage_model_n = second_stage->get_model_n();
    unsigned next_stage_model_i;

#ifdef EVENLY_ASSIGN
    next_stage_model_i = static_cast<unsigned>(
        (index_pred - first_stage_pred_min) /
        (first_stage_pred_max + 1 - first_stage_pred_min) * next_stage_model_n);
#else
    if (index_pred >= key_n)
    {
      // if(index_pred >= max_addr) {
      next_stage_model_i = next_stage_model_n - 1;
    }
    else if (index_pred < 0)
    {
      next_stage_model_i = 0;
    }
    else
    {
      next_stage_model_i =
          static_cast<unsigned>(index_pred / key_n * next_stage_model_n);
    }
#endif
#if 0
    if(next_stage_model_n > 12) {
      fprintf(stderr,"get wrong next stage n: %u",next_stage_model_n);
      assert(false);
    }
#endif
    return next_stage_model_i;
  }
#endif

private:
  const RMIConfig config;

public:
  std::vector<double> all_keys; // not valid after calling finish_insert
  std::vector<learned_addr_t>
      all_addrs;                              // not valid after calling finish_insert
  std::map<double, learned_addr_t> addrs_map; // XD: I add this
  learned_addr_t max_addr = 0;
  bool scale = false;

  unsigned key_n;

  double first_stage_pred_max, first_stage_pred_min;

#ifdef LRfirst
  LRStage *first_stage;
#elif defined(BestModel)
  BestStage *first_stage;
#else
  NNStage<Weight_T> *first_stage;
#endif
  LRStage *second_stage;
};

template <class Weight_T>
class RMIMixTop
{
public:
  RMIMixTop(const RMIConfig &config)
      : config(config)
  {
    assert(config.stage_configs.size() == 2);
    assert(config.stage_configs.front().model_n == 1);
    assert(config.stage_configs[0].model_type ==
           RMIConfig::StageConfig::MixTopModel);
    assert(config.stage_configs[1].model_type ==
           RMIConfig::StageConfig::LinearRegression);

    // #ifdef EVENLY_ASSIGN
    //     COUT_THIS("RMI use new-dispatch!");
    // #else
    //     COUT_THIS("RMI use original-dispatch!");
    // #endif
    // init models stage by stage

    first_stage = new MixTopStage<Weight_T>(
        1,
        1,
        config.stage_configs[0].mix_top_config.width,
        config.stage_configs[0].mix_top_config.depth,
        config.stage_configs[0].mix_top_config.weight_dir,
        config.stage_configs[0].mix_top_config.nn_ranges);
    second_stage = new LRStage(config.stage_configs[1].model_n);
  }

  ~RMIMixTop()
  {
    delete first_stage;
    delete second_stage;
  }

  RMIMixTop(const RMIMixTop &) = delete;
  RMIMixTop(RMIMixTop &) = delete;

  void insert(const double key) { all_keys.push_back(key); }

  void insert_w_idx(const double key, learned_addr_t addr)
  {
    all_keys.push_back(key);
    all_addrs.push_back(addr);
  }

  void finish_insert()
  {
    assert(all_keys.size() > 0);

    key_n = all_keys.size();

    sort(all_keys.begin(), all_keys.end());

    // extract <key,index> of unique keys
    double prev_unique_key = all_keys.front();
    std::vector<double> unique_keys{all_keys.front()};
    std::vector<learned_addr_t> unique_indexes{0};

    for (int index = 0; index < all_keys.size(); ++index)
    {
      double this_key = all_keys[index];
      if (this_key != prev_unique_key)
      {
        prev_unique_key = this_key;
        unique_keys.push_back(this_key);
        unique_indexes.push_back(index);
      }
    }

    COUT_THIS("number of unique key to RMI: " << unique_keys.size());

    // feed all data to the only model in the 1st stage
    first_stage->reset_data();
    second_stage->reset_data();

    for (int i = 0; i < unique_keys.size(); ++i)
    {
      first_stage->data_in.front().first.push_back(unique_keys[i]);
      first_stage->data_in.front().second.push_back(unique_indexes[i]);
    }

    // prepare 1st stage model with fed in data
    for (int model_i = 0; model_i < first_stage->get_model_n(); ++model_i)
    {
      std::vector<double> &uni_keys = first_stage->data_in[model_i].first;
      std::vector<learned_addr_t> &uni_indexes =
          first_stage->data_in[model_i].second;

      first_stage->prepare(uni_keys,
                           uni_indexes,
                           model_i,
                           first_stage_pred_max,
                           first_stage_pred_min);

      for (int i = 0; i < uni_keys.size(); ++i)
      {
        double index_pred = first_stage->predict(uni_keys[i], model_i);
        unsigned next_stage_model_i = pick_next_stage_model(index_pred);
        second_stage->assign_data(
            uni_keys[i], uni_indexes[i], next_stage_model_i);
        assert(false);
        this->addrs_map.insert(std::make_pair(uni_keys[i], uni_indexes[i]));
      }
    }

    // prepare 2st stage model with fed in data
    for (int model_i = 0; model_i < second_stage->get_model_n(); ++model_i)
    {
      std::vector<double> &keys = second_stage->data_in[model_i].first;
      std::vector<learned_addr_t> &indexes =
          second_stage->data_in[model_i].second;

      // let it track the errors itself
      second_stage->prepare_last(keys, indexes, model_i);
    }

    first_stage->data_in.clear();
    second_stage->data_in.clear();
    all_keys.clear();
    all_addrs.clear();
    addrs_map.clear();
  }

  void predict_pos(const double key,
                   learned_addr_t &pos,
                   learned_addr_t &error_start,
                   learned_addr_t &error_end)
  {
    double index_pred = first_stage->predict(key, 0);
    unsigned next_stage_model_i = pick_next_stage_model(index_pred);
    second_stage->predict_last(
        key, pos, error_start, error_end, next_stage_model_i);
  }

  // private:
  inline unsigned pick_next_stage_model(double index_pred)
  {
    unsigned next_stage_model_n = second_stage->get_model_n();
    unsigned next_stage_model_i;

#ifdef EVENLY_ASSIGN
    assert(false);
    next_stage_model_i = static_cast<unsigned>(
        (index_pred - first_stage_pred_min) /
        (first_stage_pred_max + 1 - first_stage_pred_min) * next_stage_model_n);
#else
    if (index_pred >= key_n)
    {
      next_stage_model_i = next_stage_model_n - 1;
    }
    else if (index_pred < 0)
    {
      next_stage_model_i = 0;
    }
    else
    {
      next_stage_model_i =
          static_cast<unsigned>(index_pred / key_n * next_stage_model_n);
    }
#endif
    return next_stage_model_i;
  }

private:
  const RMIConfig config;

public:
  std::vector<double> all_keys; // not valid after calling finish_insert
  std::vector<learned_addr_t>
      all_addrs; // not valid after calling finish_insert
  std::map<double, learned_addr_t> addrs_map;
  // learned_addr_t              max_addr = 0;
  // bool scale = false;

  // unsigned key_n;
  learned_addr_t key_n;

  double first_stage_pred_max, first_stage_pred_min;

  MixTopStage<Weight_T> *first_stage;
  LRStage *second_stage;
};

#endif // RMI_H
