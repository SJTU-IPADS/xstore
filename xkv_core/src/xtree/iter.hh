#pragma once

#include "./mod.hh"

#include "../iter_trait.hh"

namespace xstore {

namespace xkv {

namespace xtree {

/*!
  Note: currently, keys in the same node is not sorted.
  Consequently, we need to sort it otherwise
 */
template <usize N, typename V>
struct XTreeIter : public KeyIterTrait<XTreeIter<N, V>, XTree<N, V>> {
  using Self = XTreeIter<N, V>;

  Option<XNodeKeys<N>> cur_node = {};
  XNode<N, V> *cur_node_ptr = nullptr;
  XNode<N, V> *next_ptr = nullptr;
  usize idx = 0; // idx in the cur node

  static auto from_impl(XTree<N, V> &kv) -> Self { return Self(kv); }

  XTreeIter(XTree<N, V> &kv) { this->seek(0, kv); }

  // impl traits
  auto begin_impl() {}

  auto next_impl() {
    this->idx += 1;
    this->seek_idx();
    if (this->idx != N) {
      return;
    }

    this->read_from(this->next_ptr);

    if (this->has_next()) {
      idx = 0; // reset
      this->seek_idx();
    }
  }

  auto has_next_impl() -> bool {
    if (this->cur_node) {
      return true;
    } else {
      return false;
    }
  }

  auto cur_key_impl() -> KeyType { return this->cur_node.value().get_key(idx); }

  auto read_from(XNode<N, V> *node) {
    if (node != nullptr) {
      // read
    retry:
      auto seq = node->keys.seq;
      this->next_ptr = node->next;
      r2::compile_fence();
      this->cur_node = node->keys.get_payload();
      if (unlikely(node->keys.seq != seq)) {
        goto retry;
      }

      // re-set next ptr
      this->cur_node_ptr = node;
    } else {
      this->cur_node = {};
      this->cur_node_ptr = nullptr;
    }
  }

  auto seek_impl(const KeyType &k, XTree<N, V> &kv) {
    auto node = kv.find_leaf(k);
    this->read_from(node);

    this->idx = 0;
    this->seek_idx();

    ASSERT(this->idx != N);
  }

  auto seek_idx() {
    while (this->idx < N &&
           this->cur_node.value().get_key(idx) == kInvalidKey) {
      this->idx += 1;
    }
  }

  auto opaque_val_impl() -> u64 {
    return reinterpret_cast<u64>(this->cur_node_ptr);
  }
};
} // namespace xtree
} // namespace xkv
} // namespace xstore
