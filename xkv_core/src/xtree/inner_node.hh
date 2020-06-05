#pragma once

#include <algorithm>
#include <limits>

#include "../../../deps/r2/src/common.hh"

#include "./xnode.hh"

namespace xstore {

namespace xkv {

namespace xtree {

using namespace r2;

using raw_ptr_t = char *;

template <usize N> struct TreeInner {
  static_assert(std::numeric_limits<u8>::max() >= N,
                "The number of keys is too large for XNode");
  u64 keys[N];
  u64 up_key = kInvalidKey;             // temporally store the up key for split usage
  raw_ptr_t childrens[N + 1];           // inner nodes has to store one more children
  u8 num_keys = 0;

  TreeInner() {
    // zeroing
    std::fill_n(childrens, N, nullptr);
  }

  TreeInner(const u64 &k, const raw_ptr_t &l, const raw_ptr_t &r) : num_keys(1) {
    this->keys[0] = k;
    this->childrens[0] = l;
    this->childrens[1] = r;
    this->num_keys = 1;
  }

  inline auto find_children(const u64 &k) -> raw_ptr_t {
    return this->childrens[this->find_children_idx(k)];
  }

  inline auto find_children_idx(const u64 &k) -> uint {
    uint i = 0;
    for (; i < this->num_keys; ++i) {
      if (k < this->keys[i]) {
        break;
      }
    }
    return i;
  }

  template <typename V>
  TreeInner<N> *insert(const u64 &key, const V &v,
                       int depth, bool &whether_split,
                       XNode<N,V> *candidate
                       ) {
    ASSERT(depth >= 1);

    auto k = this->find_children_idx(key);
    auto child = this->childrens[k];

    // init the data structure for split
    raw_ptr_t to_insert = nullptr;
    u64       up_key = 0;

    if (depth == 1) {
      // the underlying is the leaf
      auto leaf = reinterpret_cast<XNode<N, V> *>(child);
      if (leaf->insert(key,v,candidate)) {
        whether_split = true;

        to_insert = reinterpret_cast<raw_ptr_t>(candidate);
        up_key    = candidate->get_key(0);
      }
    } else {
      auto in = reinterpret_cast<TreeInner<N> *>(child);
      auto new_in = in->template insert<V>(key, v, depth - 1, whether_split, candidate);
      if (new_in != nullptr) {
        to_insert = reinterpret_cast<raw_ptr_t>(new_in);
        up_key = new_in->up_key;
      }
    }

    // insert to me
    if (to_insert != nullptr) {
      ASSERT(up_key != kInvalidKey);
      ASSERT(false) << " not implemented";


    }
    return nullptr;
  }

  inline auto empty() -> bool { return num_keys == 0; }

  inline auto full() -> bool { return num_keys == N; }

  /*!
    Copy K-addrs after *n) to the next
  */
  void copy_after_n_to(TreeInner<N> *next, const uint &n) {
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
