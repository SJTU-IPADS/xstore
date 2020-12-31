#pragma once

#include "../alloc_trait.hh"
#include "../kv_trait.hh"

#include "./inner_node.hh"

namespace xstore {

namespace xkv {

namespace xtree {

/*!
  N : fanout
  V : the value type
 */
template<usize N, typename K, typename V>
struct XTree : public KVTrait<XTree<N, K, V>, K, V>
{

  using Inner = TreeInner<N, K>;
  using Leaf = XNode<N, K, V>;

  // data structure
  int depth = 0;
  raw_ptr_t root = nullptr;
  static __thread Leaf* pre_alloc_leaf_node;

  // impl the public trait
  auto get_impl(const K& k) -> ::r2::Option<V>
  {
    auto leaf = this->find_leaf(k);
    ASSERT(leaf != nullptr);
    auto idx = leaf->search(k);
    if (idx) {
      return leaf->get_value_raw(idx.value());
    }
    return {};
  }

  auto find_leaf(const K& k) -> Leaf*
  {
    auto cur_node = root;
    auto cur_depth = depth;

    while (cur_depth != 0) {
      // traversing the inner nodes
      Inner* inner = reinterpret_cast<Inner*>(cur_node);
      auto idx = inner->find_children_idx(k);
      cur_node = inner->find_children(k);
      // down to the next
      cur_depth -= 1;
    }
    return reinterpret_cast<Leaf*>(cur_node);
  }

  auto insert_impl(const K& k, const V& v)
  {
    ::xstore::xkv::TrivalAlloc<sizeof(Leaf)> alloc;
    this->insert_w_alloc(k, v, alloc);
  }

  template<class Alloc>
  auto insert_w_alloc(const K& k, const V& v, Alloc& alloc) -> bool
  {
    // 1. initialize the pre_alloc_leaf_node
    if (unlikely(this->pre_alloc_leaf_node == nullptr)) {
      init_pre_alloced_leaf(alloc);
    }

    // 2. then do the insert
    auto ret = this->insert_core(k, v, this->pre_alloc_leaf_node);

    // leaf split
    if (ret) {
      // take out the current leaf node
      this->take_pre_alloced_leaf(alloc);
    }
    return ret;
  }

  /*!
    alloc: allocator that implements AllocTrait<sizeof(Leaf)>
    \ret: whether the underlying leaf has split
   */
  auto insert_core(const K& k, const V& v, Leaf* new_leaf) -> bool
  {
    bool ret = false;
    if (unlikely(this->root == nullptr)) {
      this->root = reinterpret_cast<raw_ptr_t>(new_leaf);
      ASSERT(new_leaf != nullptr);
      ret = true;
    }

    if (unlikely(this->depth == 0)) {
      // root is the now
      auto old_leaf = reinterpret_cast<Leaf*>(this->root);
      ASSERT(new_leaf != nullptr);

      // a split happens
      if (old_leaf->insert(k, v, new_leaf)) {
        //       <-  r  ->
        //  old_leaf   new_leaf
        this->root = reinterpret_cast<raw_ptr_t>(
          new Inner(new_leaf->get_key(0),
                    reinterpret_cast<raw_ptr_t>(old_leaf),
                    reinterpret_cast<raw_ptr_t>(new_leaf)));
        this->depth += 1;
        ret = true;
      }

    } else {
      bool whether_split = false;
      auto new_root = reinterpret_cast<Inner*>(this->root)
                        ->insert(k, v, this->depth, whether_split, new_leaf);
      if (new_root != nullptr) {
        this->depth += 1;
        this->root = reinterpret_cast<raw_ptr_t>(
          new Inner(reinterpret_cast<Inner*>(new_root)->up_key,
                    reinterpret_cast<raw_ptr_t>(this->root),
                    reinterpret_cast<raw_ptr_t>(new_root)));
      }
      ret = whether_split;
    }
    return ret;
  }

  /*!
    alloc: allocator that implements AllocTrait<sizeof(Leaf)>
    \ret: the leaf
   */
  template<class Alloc>
  auto init_pre_alloced_leaf(Alloc& alloc)
  {
    pre_alloc_leaf_node =
      new (reinterpret_cast<Leaf*>(alloc.alloc().value())) Leaf();
  }

  template<class Alloc>
  auto take_pre_alloced_leaf(Alloc& alloc) -> Leaf*
  { // first fill the cached_copy of nodes
    if (unlikely(pre_alloc_leaf_node == nullptr)) {
      init_pre_alloced_leaf(alloc);
      ASSERT(pre_alloc_leaf_node != nullptr);
    }
    auto ret = pre_alloc_leaf_node;
    init_pre_alloced_leaf(alloc); // re-set
    return ret;
  }

  /*!
    Tree sz of the inner nodes
   */
  auto sz_inner() -> usize { return sz_inner_impl(this->root, this->depth); }

  auto sz_all() -> std::vector<usize>
  {
    std::vector<usize> ret(this->depth, 0);
    sz_inner_all_impl(this->root, this->depth, ret);
    return ret;
  }

  void sz_inner_all_impl(const raw_ptr_t& node,
                         const int& depth,
                         std::vector<usize>& all)
  {
    if (node != nullptr) {
      if (depth > 0) {
        all[depth - 1] += sizeof(Inner);
        Inner* inner = reinterpret_cast<Inner*>(node);

        for (uint i = 0; i <= inner->num_keys; ++i) {
          sz_inner_all_impl(inner->childrens[i], depth - 1, all);
        }
      }
    }
  }

  auto sz_inner_impl(const raw_ptr_t& node, const int& depth) -> usize
  {
    auto cur_sz = 0;
    if (node != nullptr) {
      if (depth > 0) {
        cur_sz += sizeof(Inner);
        Inner* inner = reinterpret_cast<Inner*>(node);

        for (uint i = 0; i <= inner->num_keys; ++i) {
          cur_sz += sz_inner_impl(inner->childrens[i], depth - 1);
        }
      }
    }
    return cur_sz;
  }
};

// static member init
template<usize N, typename K, typename V>
__thread XNode<N, K, V>* XTree<N, K, V>::pre_alloc_leaf_node = nullptr;
} // namespace xtree
} // namespace xkv

} // namespace xstore
