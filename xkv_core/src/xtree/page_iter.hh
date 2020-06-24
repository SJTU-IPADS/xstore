#pragma once

#pragma once

#include "./mod.hh"

#include "../iter_trait.hh"

namespace xstore {

namespace xkv {

namespace xtree {

/*!
  Unlike iterator in iter.hh, which finds all (K,V) in XTree,
  Page iterator finds all leaf node in XTree, started from the seek key

  The unit test file is implemented in a seperate module,
  xcache/tests/test_sampler.cc
 */
template <usize N, typename V>
struct XTreePageIter : public KeyIterTrait<XTreePageIter<N, V>, XTree<N, V>> {
  using Self = XTreePageIter<N, V>;

  Option<XNodeKeys<N>> cur_node = {};
  XNode<N, V> *cur_node_ptr = nullptr;
  usize logic_page_id = 0;

  static auto from_impl(XTree<N, V> &kv) -> Self { return Self(kv); }

  XTreePageIter(XTree<N, V> &kv) : logic_page_id(0) { this->seek(0, kv); }

  // impl traits
  auto begin_impl() {}

  auto next_impl() {
    if (this->cur_node_ptr) {
      this->cur_node_ptr = this->cur_node_ptr->next;
      logic_page_id += 1;
      this->read_from(this->cur_node_ptr);
    }
  }

  auto has_next_impl() -> bool { return this->cur_node_ptr != nullptr; }

  auto cur_key_impl() -> KeyType { return static_cast<u64>(logic_page_id); }

  auto seek_impl(const KeyType &k, XTree<N, V> &kv) {
    this->cur_node_ptr = kv.find_leaf(k);
    this->logic_page_id = 0;
    this->read_from(this->cur_node_ptr);
  }

  auto opaque_val_impl() -> u64 {
    return reinterpret_cast<u64>(this->cur_node_ptr);
  }

  auto read_from(XNode<N, V> *node){
    if (node != nullptr) {
      // read
    retry:
      auto seq = node->keys.seq;
      r2::compile_fence();
      this->cur_node = node->keys.get_payload();
      if (unlikely(node->keys.seq != seq)) {
        goto retry;
      }
    }
  }
};
} // namespace xtree
} // namespace xkv
} // namespace xstore
