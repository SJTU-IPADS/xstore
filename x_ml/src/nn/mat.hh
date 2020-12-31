#pragma once

#include <mkl.h>
#include <mkl_spblas.h>

#include <assert.h>

#include "../../../deps/r2/src/common.hh"

/*!
    utilities to fast multiplate matrixs using MKL
 */

namespace xstore {
namespace xml {

using namespace r2;
template<typename Float>
struct Matrix
{
  /*!
    We assume the data are stored in row-major in the ptr
   */
  Float* ptr;
  const usize nrow;
  const usize ncol;

  Matrix(Float* d, const usize& nr, const usize& nc)
    : ptr(d)
    , nrow(nr)
    , ncol(nc)
  {}

  inline auto at(const usize& r, const usize& c) -> Float&
  {
    return ptr[r * ncol + c];
  }

  auto payload() const -> usize
  {
    return sizeof(Float) * (this->ncol * this->nrow);
  }

  inline auto man_mult(Matrix& b, Matrix& c) -> Matrix
  {
    assert(this->ncol == b.nrow);
    assert(this->nrow == c.nrow);
    assert(c.ncol == b.ncol);
    for (uint r = 0; r < c.nrow; ++r) {
      for (uint col = 0; col < c.ncol; ++col) {
        Float temp = 0;
        for (uint i = 0; i < this->ncol; ++i) {
          temp += this->at(r, i) * b.at(i, col);
        }
        c.at(r, col) += temp;
      }
    }
    return Matrix(c);
  }

  auto mult(const Matrix& b, Matrix& c) -> Matrix
  {
    assert(this->ncol == b.nrow);
    assert(this->nrow == c.nrow);
    assert(c.ncol == b.ncol);

    // use MKL to multiple
    cblas_dgemm(CblasColMajor,
                CblasNoTrans,
                CblasNoTrans,
                this->nrow,
                b.ncol,
                this->ncol,
                1.0,
                this->ptr,
                this->nrow,
                b.ptr,
                b.nrow,
                1.0,
                c.ptr,
                c.nrow);
    return Matrix(c);
  }
};

}
}