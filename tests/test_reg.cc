#include <gtest/gtest.h>

#include "../xcli/server/reg.hh"

#include "../x_ml/src/lr/mod.hh"
#include "../xcache/src/page_tt_iter.hh"

using namespace xstore;

namespace test {

using XCache = LocalTwoRMI<LR, LR, XKey>;
using Reg = XCacheReg<XCache, XCacheTT>;

TEST(XLib, reg) {
  Reg reg;
  reg.set_n_models(1000).create_model_tt();

  ASSERT_EQ(reg.model->size(), 1000);
  ASSERT_EQ(reg.tts->size(), reg.model->num_subs());
}

} // namespace test

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
