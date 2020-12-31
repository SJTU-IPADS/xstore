#pragma once

#include <algorithm>

#include "./iter.hh"

namespace xstore {

namespace xkv {
namespace xtree {
template<usize N, typename KeyType, typename V>

/*!
    This iterator provides a truely sorted iterator.
    Yet, it only intends to provide statics counting,
    not optimized for **performance critical** tasks.
 */
struct XTreeSIter
  : public KeyIterTrait<XTreeSIter<N, KeyType, V>,
                        XTree<N, KeyType, V>,
                        KeyType>
{
  using Self = XTreeSIter<N, KeyType, V>;

  ::r2::Option<XNodeKeys<N, KeyType>> cur_node = {};
  XNode<N, KeyType, V>* cur_node_ptr = nullptr;
  XNode<N, KeyType, V>* next_ptr = nullptr;
  usize idx = 0;     // cur idx in the cur node
  usize counter = 0; // how many keys have been iterated

  static auto from_impl(XTree<N, KeyType, V>& kv) -> Self { return Self(kv); }

  XTreeSIter(XTree<N, KeyType, V>& kv) { this->seek(KeyType::min(), kv); }

  // impl traits
  auto begin_impl()
  {
    // FIXME: not implemented
    LOG(4) << "warning: tree iter (nil) argument begin has not implemented";
    // counter = 0;
    // idx = 0;
    // this->seek(KeyType::min(),kv);
  }

  auto next_impl()
  {
    this->idx += 1;
    this->seek_idx();
    if (this->idx != N) {
      this->counter += 1;
      return;
    }

    this->read_from(this->next_ptr);

    if (this->has_next()) {
      idx = 0; // reset
      this->seek_idx();
      this->counter += 1;
    }
  }

  auto has_next_impl() -> bool
  {
    if (this->cur_node) {
      return true;
    } else {
      return false;
    }
  }

  auto cur_key_impl() -> KeyType { return this->cur_node.value().get_key(idx); }

  auto read_from(XNode<N, KeyType, V>* node)
  {
    if (node != nullptr) {
      // read
    retry:
#if XNODE_KEYS_ATOMIC
      // do atomic checks
      auto seq = node->keys.seq;
#endif
      this->next_ptr = node->next;
      r2::compile_fence();

      // sort the keys in a node
      std::vector<KeyType> temp_keys;
      for (uint i = 0; i < N; ++i) {
        temp_keys.push_back(node->keys_ptr()->get_key(i));
      }
      std::sort(temp_keys.begin(), temp_keys.end());
      if (!this->cur_node) {
        this->cur_node = *(node->keys_ptr());
      }

      for (uint i = 0; i < N; ++i) {
        this->cur_node.value().keys[i] = temp_keys[i];
      }

#if XNODE_KEYS_ATOMIC
      if (unlikely(node->keys.seq != seq)) {
        goto retry;
      }
#endif
      // re-set next ptr
      this->cur_node_ptr = node;
    } else {
      this->cur_node = {};
      this->cur_node_ptr = nullptr;
    }
  }

  auto seek_impl(const KeyType& k, XTree<N, KeyType, V>& kv)
  {
    auto node = kv.find_leaf(k);
    this->read_from(node);

    this->counter = 0;
    this->idx = 0;
    this->seek_idx();

    ASSERT(this->idx != N);
  }

  auto seek_idx()
  {
    while (this->idx < N &&
           this->cur_node.value().get_key(idx) == KeyType(kInvalidKey)) {
      this->idx += 1;
    }
  }

  auto opaque_val_impl() -> u64
  {
    // return reinterpret_cast<u64>(this->cur_node_ptr);
    return counter;
  }
};
} // namespace xtree
} // namespace xkv
} // namespace xstore