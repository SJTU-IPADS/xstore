#pragma once

#include "./mod.hh"

#include "../iter_trait.hh"

namespace xstore {

namespace xkv {

namespace xtree {

template <usize N, typename V>
struct XTreeIter : public KeyIterTrait<XTreeIter<N, V>, XTree<N, V>> {
  using Self = XTreeIter<N, V>;

  Option<XNodeKeys<N, V>> cur_node = {};
  XNode<K, V> *cur_node_ptr = nullptr;
  XNode<N, V> *next_ptr = nullptr;
  usize idx = 0; // idx in the cur node

  static auto from_impl(XTree<N, V> &kv) -> Self { return Self(kv); }

  XTreeIter(XTree<N, V> &kv) { this->begin(); }

  // impl traits
  auto begin_impl() {}

  auto next_impl() {
    idx += 1;
    if (idx < N && this->cur_node.get_key(idx) != kInvalidKey) {
      return;
    } else {
      goto retry;
    }
    ASSERT(idx == N);

    // increment to the next node
    if (next_ptr != nullptr) {
      // TODO:xx
    } else {
      cur_node = {};
    }
  }

  auto has_next_impl() -> bool { return this->cur_node; }

  auto cur_key_impl() -> KeyType { return this->cur_node.value().get_key(idx); }
};
} // namespace xtree
} // namespace xkv
} // namespace xstore
