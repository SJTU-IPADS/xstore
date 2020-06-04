#pragma once

#include <algorithm>
#include <limits>

#include "../../../deps/r2/src/common.hh"

namespace xstore {

namespace xkv {

namespace xtree {

using namespace r2;

template <usize N> struct TreeInner {
  static_assert(std::numeric_limits<u8>::max() >= N,
                "The number of keys is too large for XNode");
  u64 keys[N];
  u64 up_key;             // temporally store the up key for split usage
  void *childrens[N + 1]; // inner nodes has to store one more children
  u8 num_keys = 0;

  TreeInner() {
    // zeroing
    std::fill_n(childrens, N, nullptr);
  }

  inline auto empty() -> bool { return num_keys == 0; }

  /*!
    Copy K-addrs after *n) to the next
  */
  void copy_last_n_to(TreeInner<N> *next, const uint &n) {
    ASSERT(next->empty());
    uint copied = 0;
    for (uint i = n; i < this->num_keys; ++i, copied += 1) {
      copied += 1;
      next->keys[i - n] = this->keys[i];
      next->childrens[i - n] = this->childrens[i];
    }
    next->num_keys = copied;
    // set the end children
    next->childrens[next->num_keys] = this->childrens[this->num_keys];
  }
};

} // namespace xtree

} // namespace xkv

} // namespace xstore
