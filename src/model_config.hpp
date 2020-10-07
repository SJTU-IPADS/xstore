#pragma once

#include "cpptoml/include/cpptoml.h"
#include "mousika/rmi.h"

#include "common.hpp"

namespace fstore {

/*!
  Load the model configuration, and returns a learned index configuration.
*/

typedef std::shared_ptr<cpptoml::table> config_handle_t;
class ModelConfig {
 public:
  static RMIConfig load(const std::string &config) {
    return load_internal(cpptoml::parse_file(config));
  }

  static RMIConfig load_internal(const config_handle_t &handle) {
    RMIConfig config;
    auto tarr = handle->get_table_array("stages");
    for(auto &tab : *tarr) {
      auto res = load_stage(tab);
      if(res)
        config.stage_configs.push_back(res.value());
    }
    LOG(4) << "load second stage num: " << config.stage_configs.size();
    return config;
  }

 private:
  static RMIConfig::StageConfig::model_t model_type_convert(const std::string &name) {
    if(name == "lr")
      return RMIConfig::StageConfig::LinearRegression;
    return RMIConfig::StageConfig::Unknown;
  }

  static Option<RMIConfig::StageConfig>
  load_stage(const config_handle_t &handle) {
    {
      RMIConfig::StageConfig config;
      auto type = handle->get_as<std::string>("type");
      if(!type)
        goto END;
      config.model_type = model_type_convert(*type);
      auto parameter = handle->get_as<u32>("parameter");
      if(!parameter)
        goto END;
      config.model_n = *parameter;
      return Option<RMIConfig::StageConfig>{ config };
    }
 END:
    return {};
  }
};



} // end namespace fstore
