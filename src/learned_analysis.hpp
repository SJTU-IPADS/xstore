#pragma once

#include "common.hpp"
#include "mega_iter.hpp"

#include "utils/all.hpp"
#include "datastream/stream.hpp"

#include <functional>
#include <array>

namespace fstore {

/*!
  This file has different tools for analysis the behavior of the learned index.
*/
class LearnedAnalyzer {
 public:
  /*!
    Dump the learned DataMap to python files.
    \param key_transform: whether the learned index's key has been transformed using MegaFaker.
  */
  template <typename T>
  static uint dump_cdf_py(datastream::StreamIterator<u64,T> &it,
                          LearnedRangeIndexSingleKey<u64,u64> &li,
                          bool key_transform = true,
                          const std::string &pos_file = "data.py",
                          const std::string &start_file = "start.py",
                          const std::string &end_file = "end.py") {

    using namespace utils;

    DataMap<u64,u64> predicts_cdf("predicts");
    DataMap<u64,u64> predicts_start("start");
    DataMap<u64,u64> predicts_end("end");

    auto res = analyze_stream<T>(it,[&](u64 k,T v) {
                                      /**
                                       * transform the key,
                                       * because we used the transformed data to train the model
                                       */
                                      k = key_transform ? MegaFaker::encode(k) : k;
                                      auto predicts = li.predict(k);
                                      /**
                                       * transform the key back,
                                       * because the DataMap should based on the original key-space
                                       */
                                      k = key_transform ? MegaFaker::decode(k) : k;
                                      predicts_cdf.insert(k,predicts.pos);
                                      predicts_start.insert(k,predicts.start);
                                      predicts_end.insert(k,predicts.end);
                                    });
    FILE_WRITE(pos_file,std::ofstream::out)
        << predicts_cdf.dump_as_np_data();
    FILE_WRITE(start_file,std::ofstream::out)
        << predicts_start.dump_as_np_data();
    FILE_WRITE(end_file,std::ofstream::out)
        << predicts_end.dump_as_np_data();
    return res;
  }

  /*!
    Dump the real mega_id distribution to a cdf file.
  */
  static uint dump_real_cdf_py(datastream::StreamIterator<u64,mega_id_t> &it,
                               const std::string &data_file = "real.py") {
    utils::DataMap<u64,u64> cdf("real");
    auto res = analyze_stream<mega_id_t>(it,[&](u64 k,mega_id_t id) {
                                              if(MegaFaker::is_fake(k))
                                                return;
                                              k = MegaFaker::decode(k);
                                              cdf.insert(k,id);
                                            });
    FILE_WRITE(data_file,std::ofstream::out)
        << cdf.dump_as_np_data();
    return res;
  }

  /*!
    Return all the prediction that fits in one page.
  */
  template <typename T,class MP>
  static std::vector<u64> get_fitin_keys(datastream::StreamIterator<u64,T> &it,
                                         LearnedRangeIndexSingleKey<u64,u64> &li,
                                         MP &mp,
                                         bool key_transform = true) {
    std::vector<u64> keys;
    analyze_stream<T>(it,[&](u64 k,T) {
                           k = key_transform ? MegaFaker::encode(k) : k;
                           auto predicts = li.predict(k);
                           if(mp.within_page(predicts))
                             keys.push_back(MegaFaker::decode(k));
                         });
    return keys;
  }

  /*! Return the distribution of page span */
  template <typename T,typename V,class MP>
  static utils::DataMap<u64,u64> get_page_span(datastream::StreamIterator<u64,T> &it,
                                           LearnedRangeIndexSingleKey<u64,V> &li,
                                           MP &mp,
                                           bool key_transform = true) {
    utils::DataMap<u64,u64> span_cdf("page_span");
    u64 count = 0;
    analyze_stream<T>(it,[&](u64 k,T) {
                           k = key_transform ? MegaFaker::encode(k) : k;
                           auto span = mp.page_span(li.predict(k));
                           span_cdf.insert(count,span);
                           count += 1;
                         });
    return span_cdf;
  }

  template <typename T,typename V>
  static utils::CDF<u64> logic_error_distribution(datastream::StreamIterator<u64,T> &it,
                                           LearnedRangeIndexSingleKey<u64,V> &li) {
    utils::CDF<u64> res("error");
    u64 count = 0;

    analyze_stream<T>(it,[&](u64 k,T) {
                           auto predict = li.predict(k);
                           //auto logic_addr = li.get_logic_addr(k);
                           auto logic_addr = static_cast<i64>(li.get(k));
                           res.insert(std::abs(logic_addr - predict.pos));
                         });
    res.finalize();
    return res;
  }

  template <typename Model>
  static utils::DataMap<u64,i64> get_error_report(Model &model) {
    utils::DataMap<u64,i64> report("model");
    return report;
  }

 private:
  template <typename T,class ...Fns>
  static uint analyze_stream(datastream::StreamIterator<u64,T> &it,
                             Fns... fns) {
    std::array<std::function<void (u64,T)>, sizeof...(fns)> fl = {fns...};
    uint count = 0;
    for(it.begin();it.valid();it.next(),count += 1) {
      for (auto f : fl) {
        f(it.key(),it.value());
      }
    }
    return count;
  }
};

} // end namespace fstore
