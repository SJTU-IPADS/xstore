#pragma once

#include "smart_cache.hpp"
#include "utils/all.hpp"

#include <algorithm>
#include <limits>

namespace fstore {

class KeyDistribution
{
public:
private:
  std::vector<u64> all_spans;
};

class KeySpanStatics
{
public:
  /**
   * Credits: The alogrithms avoids overflow, and comes from:
   *  https://stackoverflow.com/questions/1930454/what-is-a-good-solution-for-calculating-an-average-where-the-sum-of-all-values-e
   */
  void add(u64 data)
  {
    max_span = std::max(max_span, data);
    min_span = std::min(min_span, data);

    average_span += (data - average_span) / counter;
    counter += 1;
  }

  double average() const { return average_span; }

  u64 max() const { return max_span; }

  u64 min() const { return counter == 1 ? 0 : min_span; }

  u64 total_keys() const { return counter - 1; }

  std::string to_str() const
  {
    std::stringstream ss;
    ss << "Key span of [" << total_keys() << "] keys: "
       << "min: " << utils::format_value(min()) << ";"
       << "max: " << utils::format_value(max()) << ";"
       << "average: " << utils::format_value(average()) << ".";
    return ss.str();
  }

private:
  double average_span = 0;
  u64 max_span = 0;
  u64 min_span = std::numeric_limits<u64>::max();
  u64 counter = 1;
};

template<class SC, Class Iter>
class SCAnalysis
{
public:
  static KeySpanStatics get_span_statics(SC& sc, Iter& keys)
  {

    KeySpanStatics ret;

    for (keys.begin(); keys.valid(); keys.next()) {
      auto predict = sc.get_predict(keys.key());
      ASSERT(predict.end >= predict.start)
        << "get predict range: [" << predict.start << "," << predict.end
        << "].";
      ret.add(predict.end - predict.start + 1);
    }
    return ret;
  }

  /// given a learned index
  /// output the distribution of its model's number of training-set
  static utils::CDF<u64> get_model_training_cdf(SC &sc) {
    utils::CDF<u64> cdf("model_cdf");
    u64 max = 0;
    for (auto i = 0;i < sc.index->rmi.second_stage->get_model_n();++i) {
      auto& model = sc.index->get_lr_model(i);

      cdf.insert(model.num_training_set);
      max = std::max(static_cast<u64>(model.num_training_set), max);
    }
    LOG(4) << "max training num: " << max;
    cdf.finalize();
    return cdf;
  }

  static utils::CDF<i64> get_span_cdf(SC& sc, Iter& keys)
  {
    utils::CDF<i64> cdf("page_cdf");
    for (keys.begin(); keys.valid(); keys.next()) {
      auto predict = sc.get_predict(keys.key());
      ASSERT(predict.end >= predict.start);
      cdf.insert(predict.end - predict.start);
    }
    cdf.finalize();
    return cdf;
  }

  static utils::DataMap<u64, u64> get_span_data(SC& sc, Iter& keys)
  {
    utils::DataMap<u64, u64> data("page");
    for (keys.begin(); keys.valid(); keys.next()) {
      auto predict = sc.get_predict(keys.key());
      ASSERT(predict.end >= predict.start)
        << "get predict range: [" << predict.start << "," << predict.end
        << "].";
      data.insert(keys.key(), predict.end - predict.start);
    }
    return data;
  }
};

} // end namespace fstore
