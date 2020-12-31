#include <gtest/gtest.h>

#include "../src/nn/mat.hh"

namespace test {

using namespace xstore::xml;
TEST(Mat, mut)
{
  using Mat = Matrix<double>;
  std::vector<double> a({ 1, 2, 3, 4, 5, 6 });
  Mat A(a.data(), 1, 6);

  std::vector<double> b({ 1, 2, 3, 4, 5, 6 });
  Mat B(b.data(), 6, 1);

  double res = 0;
  Mat C(&res, 1, 1);

  A.mult(B, C);
  ASSERT_EQ(res, 91);

  double d = 12;
  std::vector<double> e({ 1, 2, 3, 4, 5, 6 });
  std::vector<double> f({ 0, 0, 0, 0, 0, 0 });

  Mat D(&d, 1, 1);
  Mat E(e.data(), 1, 6);
  Mat F(f.data(), 1, 6);
  auto ret = D.mult(E, F);
  r2::compile_fence();

  ASSERT_EQ(ret.ncol, F.ncol);
  ASSERT_EQ(ret.nrow, F.nrow);

  for (auto v : f) {
    LOG(4) << v;
  }
  for (uint i = 0; i < 6; ++i) {
    ASSERT_EQ(*(ret.ptr + i), 12 * (i + 1));
  }
}
}

int
main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
