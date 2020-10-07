#pragma once

#include "../server/internal/table.hpp"

#include "../src/utils/cdf.hpp"

#ifdef MODEL_COUNT
#else
#define MODEL_COUNT 0
#endif

namespace fstore {

using namespace server;

class XDirectTopLayer
{
public:
  LRModel<> dispatcher;
  std::vector<std::shared_ptr<SBM>> submodels;

#if MODEL_COUNT
  std::vector<u64> model_reference_count;
#endif

  usize max_addr;

  XDirectTopLayer(const std::string &d, int num_submodels, usize max_addr)
    : submodels(num_submodels, nullptr),
      max_addr(max_addr) {
    dispatcher.from_serialize(d);
#if MODEL_COUNT
    model_reference_count.resize(num_submodels);
    for(uint i = 0;i < model_reference_count.size();++i) {
      model_reference_count[i] = 0;
    }
#endif
  }

#if MODEL_COUNT
  ::fstore::utils::CDF<u64> report_model_selection() {
    ::fstore::utils::CDF<u64> res("");
    u64 total = 0;
    u64 max = 0;
    for(uint i = 0;i < model_reference_count.size();++i) {
      res.insert(model_reference_count[i]);
      total += model_reference_count[i];
      max = std::max(max,model_reference_count[i]);
    }
    res.finalize();
    LOG(4) << "total access time: "<< total << " " << max;
    return res;
  }
#endif

  std::shared_ptr<SBM> get_predict_model(const u64 &key) {
    return submodels[this->select_submodel(key)];
  }

  usize select_submodel(const u64& key)
  {
    auto predict = static_cast<int>(dispatcher.predict(key));
    usize ret = 0;
    if (predict < 0) {
      ret = 0;
    } else if (predict >= max_addr) {
      ret = submodels.size() - 1;
    } else {
      ret = static_cast<usize>((static_cast<double>(predict) / max_addr) *
                               submodels.size());
    }
    ASSERT(ret < submodels.size())
      << "predict: " << predict << " " << submodels.size();
    // LOG(4) << "select " << ret << " for " << key << " with predict: " <<
    // predict;
#if MODEL_COUNT
    model_reference_count[ret] += 1;
#endif
    return ret;
  }
};

}
