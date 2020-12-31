#pragma once

/*!
  Use RTM to make XTree concurrent safe
 */

#include "../../xutils/rtm.hh"

#include "./xtree/mod.hh"

namespace xstore {

namespace xkv {

using namespace xtree;
using namespace ::xstore::util;

template <usize N, typename K, typename V>
struct CXTree : public KVTrait<CXTree<N, K, V>, K, V> {
  using TreeType = XTree<N,K,V>;

  TreeType inner;
  SpinLock fallback_lock;

  CXTree() = default;

  // wrap over the KV traits

  auto get_impl(const K &k) -> ::r2::Option<V> {
    RTMScope rtm(&this->fallback_lock);
    return inner.get(k);
  }

  /*!
    This version of get() is not concurrent safe
   */
  auto unsafe_get(const K&k) -> ::r2::Option<V> {
    return inner.get(k);
  }

  auto insert_impl(const K &k, const V &v) {
    ::xstore::xkv::TrivalAlloc<sizeof(typename TreeType::Leaf)> alloc;
    this->insert_w_alloc(k, v, alloc);
  }

  template <class Alloc>
  auto insert_w_alloc(const K &k, const V &v, Alloc &alloc) -> bool {
    // 1. initialize the pre_alloc_leaf_node
    if (unlikely(inner.pre_alloc_leaf_node == nullptr)) {
      inner.init_pre_alloced_leaf(alloc);
    }

    r2::compile_fence();
    bool ret = false;
    {
      RTMScope rtm(&this->fallback_lock);
      // 2. then do the insert
      // only the insert core needs to protect in an RTM scope
      ret = this->inner.insert_core(k, v, this->inner.pre_alloc_leaf_node);
    }
    r2::compile_fence();
    //LOG(4) << "insert: " << k << " done; " << ret;

    // leaf split
    if (unlikely(ret == true)) {
      // take out the current leaf node
      this->inner.take_pre_alloced_leaf(alloc);
    }
    return ret;
  }

};

} // namespace xkv

} // namespace xstore
