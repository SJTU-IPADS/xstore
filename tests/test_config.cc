#include <gtest/gtest.h>

#include "../src/model_config.hpp"

#include <sstream>

using namespace fstore;
using namespace std;

namespace test {

TEST(Config,RMI) {

  const string template_config = "[[stages]]\n"
                                      "type = 'lr'\n"
                                      "parameter = 1\n"
                                      "\n"
                                      "[[stages]]\n"
                                      "type='lr'\n"
                                      "parameter = 12\n";

  stringstream ss(template_config);
  cpptoml::parser p{ss};
  RMIConfig config = ModelConfig::load_internal(p.parse());
  ASSERT_EQ(config.stage_configs.size(),2);
  for(auto &s : config.stage_configs) {
    ASSERT_EQ(s.model_type,RMIConfig::StageConfig::LinearRegression);
  }
}

}
