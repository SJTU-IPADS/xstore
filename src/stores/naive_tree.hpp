#pragma once

#include <algorithm>

#include "pager.hpp"
#include "node.hpp"

#include "../utils/rtm_scope.hpp"

#include "r2/src/allocator_master.hpp"

#define REMOTE 0

namespace fstore
{

namespace store
{

/*! A simple B+tree to test whether paging works for learned index
    This tree is single-threaded.
 */
class DBXTree;
template <typename KEY, typename VAL, template <class> class P = BlockPager>
class NaiveTree
{
  friend class DBXTree;
  utils::SpinLock fallback_lock;

public:
  typedef InnerNode<KEY> Node;
  typedef LeafNode<KEY, VAL, P> Leaf;

  NaiveTree() = default;

  ~NaiveTree()
  {
    // TODO: not implemented the memory free of this tree
  }

  VAL* safe_get(KEY key) {
    auto leaf = safe_find_leaf_page(key);

    if (unlikely(leaf == nullptr)) {
      // ASSERT(false);
      return nullptr;
    }
    for (uint i = 0; i < leaf->num_keys; ++i) {
      if (leaf->keys[i] == key) {
        return &(leaf->values[i]);
      }
    }
    return nullptr;
  }

  VAL* get(KEY key)
  {
    // auto leaf = safe_find_leaf_page(key);
    auto leaf = find_leaf_page(key);

    if(unlikely(leaf == nullptr)) {
      //ASSERT(false);
      return nullptr;
    }
    for (uint i = 0; i < leaf->num_keys; ++i)
    {
      if (leaf->keys[i] == key) {
        return &(leaf->values[i]);
      }
    }
    return nullptr;
  }

  usize calculate_node_sz(void *node, const usize &d) const {
    if (!node)
      return 0;

    if (d - 1 == 0)
      return sizeof(Node);

    usize sz = 0;

    auto inner = (Node*)node;
    for(uint i = 0;i < inner->num_keys;++i) {
      sz += calculate_node_sz(inner->children[i],d - 1);
    }
    sz += calculate_node_sz(inner->children[inner->num_keys],d - 1);
    return sz += sizeof(Node);
  }

  usize calculate_index_sz(const usize &d) {
    return calculate_node_sz(root,std::min(d,static_cast<usize>(depth)));
  }

  Leaf* safe_find_leaf_page(KEY key) {
    utils::RTMScope<> guard(&fallback_lock);
    return find_leaf_page(key);
  }

  u64 traverse_to_depth(KEY key, usize max_t) const {
    void* node = root;
    unsigned d = depth;
    uint index = 0;
    usize traversed = 0;
    while (d--) {
      index = 0;
      auto inner = (Node*)node;

      while (index < inner->num_keys) {
        if (key < inner->keys[index])
          break;
        ++index;
      }
      node = inner->children[index];

      traversed += 1;

      if (traversed >= max_t)
        break;
    }
    return (u64)node;
  }

  Leaf *find_leaf_page(KEY key) const
  {
    //register void* node = root;
    //register unsigned d = depth;
    void *node = root;
    unsigned d = depth;
    uint index = 0;
    //LOG(4) << "find leaf page de: " << d;
    while (d--)
    {
      index = 0;
      auto inner = (Node *)node;
      //ASSERT(inner->num_keys > 0) << d << " " << key;


      while (index < inner->num_keys)
      {
        //LOG(4) << "compare key: " << key << " " << inner->keys[index] << " ; total: " << inner->num_keys;
        if (key < inner->keys[index])
          break;
        ++index;
      }
      node = inner->children[index];
    }
    //LOG(4) << "find leaf page idx: "<< index;
    return reinterpret_cast<Leaf *>(node);
  }

  VAL *put(KEY key, const VAL &val)
  {
    auto ret = get_with_insert(key);
    *ret = val;
    return ret;
  }

  /*!
    Concurrent safe version of get with insert
   */
  inline VAL *safe_get_with_insert(u64 key) {
    {
      utils::RTMScope<> guard(&fallback_lock);
      return get_with_insert(key);
    }
  }

  inline VAL *get_with_insert(u64 key)
  {
    Option<std::pair<KEY, KEY>> up_keys = {};
    return insert(key,up_keys);
  }

  inline std::pair<VAL *, Option<std::pair<KEY,KEY>>> get_with_insert_check_split(u64 key) {
    Option<std::pair<KEY, KEY>> up_keys = {};
    auto res = insert(key, up_keys);
    return std::make_pair(res,up_keys);
  }

  inline std::pair<VAL*, Option<std::pair<KEY,KEY>>>  safe_get_with_insert_check_split(u64 key)
  {
    utils::RTMScope<> guard(&fallback_lock);
    Option<std::pair<KEY, KEY>> up_keys = {};
    auto res = insert(key, up_keys);
    return std::make_pair(res, up_keys);
  }

  inline VAL* insert(KEY key, Option<std::pair<KEY, KEY>> &up_keys)
  {

    VAL *ret = nullptr;

    if (unlikely(root == nullptr))
    {
      root = (P<Leaf>::allocate_one());
      depth = 0;
    }

    if (unlikely(depth == 0))
    {
      auto old_leaf =
        reinterpret_cast<Leaf*>(root);
      auto new_leaf = old_leaf->insert(key, &ret);
      if (new_leaf != nullptr)
      {
#if REMOTE
        using Alloc = r2::AllocatorMaster<>;
        auto temp = root;
        root = Alloc::get_thread_allocator()->alloc(sizeof(Node));
        ASSERT(root != nullptr);
        {
          Node *new_root = (Node *)root;
          new_root->keys[0] = new_leaf->keys[0];
          new_root->children[0] = temp;
          new_root->children[1] = new_leaf;
          new_root->num_keys = 1;
        }
#else
        root = new Node(new_leaf->keys[0], root, new_leaf);
#endif
        //leaf = old_leaf;
        up_keys = std::make_pair(old_leaf->start_key(), new_leaf->end_key());
        depth += 1;
      }
    }
    else
    {
      inner_insert(key, reinterpret_cast<Node *>(root), depth, &ret, up_keys);
    }

    return ret;
  }

public:
  void *root = nullptr;
  int depth = 0;

  u64 node_sz = 0;
private:

  inline Node *new_inner_node()
  {
#if REMOTE
    using Alloc = r2::AllocatorMaster<>;
    auto res = (Node *)Alloc::get_thread_allocator()->alloc(sizeof(Node));
    ASSERT(res != nullptr);
#else
    auto res = new Node();
#endif

    node_sz += sizeof(Node);
    return res;
  }

private:
  Node* inner_insert(KEY key,
                     Node* inner,
                     int d,
                     VAL** ret,
                     Option<std::pair<KEY, KEY>> &up_keys)
  {

    KEY up_key;
    Node *new_sibling = nullptr;

    auto k = find_pos(key, inner);
    void *child = inner->children[k];

    if (d == 1)
    {
      /**
       * This case, the underlying data structures is leafs
       */
      auto leaf = reinterpret_cast<Leaf *>(child);
      auto prev_seq = leaf->seq;
      auto new_leaf = leaf->insert(key, ret);

      if (new_leaf != nullptr)
      {
        //up_leaf = leaf;
        ASSERT(leaf->seq > prev_seq);
        r2::compile_fence();

        //LOG(4) << "split leaf page: " << P<Leaf>::page_id(leaf);
        Node *to_insert = inner;

        if (inner->num_keys == IN)
        {
          new_sibling = new_inner_node();
          if (new_leaf->num_keys == 1)
          {
            //ASSERT(false);
            new_sibling->num_keys = 0;
            up_key = new_leaf->keys[0];
            to_insert = new_sibling;
            k = -1;
          }
          else
          {

            unsigned threshold = (IN + 1) / 2;
            inner->copy_to(new_sibling, threshold);

            inner->num_keys = threshold - 1;

            up_key = inner->keys[threshold - 1];

            if (new_leaf->keys[0] >= up_key) {
              assert(k >= threshold);
              k -= threshold;
              to_insert = new_sibling;
            }
          }
          new_sibling->temp_insert_key() = up_key;
        } else {
        }
        if (k != -1)
        {
          for (int i = to_insert->num_keys; i > k; i--) {
            to_insert->keys[i] = to_insert->keys[i - 1];
            to_insert->children[i + 1] = to_insert->children[i];
          }
          to_insert->num_keys++;
          // the inserted key is the first key of the new leaf
          to_insert->keys[k] = new_leaf->keys[0];
          to_insert->children[k] = leaf;
        }
        to_insert->children[k + 1] = new_leaf;

        // return up keys
        up_keys = std::make_pair(leaf->start_key(), new_leaf->end_key());
      } else {
        ASSERT(prev_seq == leaf->seq)
          << "new leaf: " << new_leaf << "; prev_seq: " << prev_seq
          << " leaf seq: " << leaf->seq;
      }
    } else {
      /**
       * This case, the underlying data structures are InnerNodes
       */
      Node *cc = reinterpret_cast<Node *>(child);
      Node *new_inner = inner_insert(key, cc, d - 1, ret, up_keys);

      if (new_inner != nullptr)
      {
        Node *to_insert = inner;
        Node *child_sibling = new_inner;
        unsigned threshold = (IN + 1) / 2;

        if (inner->num_keys == IN)
        {

          new_sibling = new_inner_node();

          if (child_sibling->num_keys == 0)
          {
            //ASSERT(false);
            new_sibling->num_keys = 0;
            up_key = child_sibling->temp_insert_key();
            to_insert = new_sibling;
            k = -1;
          }
          else
          {
            inner->copy_to(new_sibling, threshold);
            /**
             * The size is threshold - 1, this is because this key is excluded from
             * this InnerNode.
             */
            inner->num_keys = threshold - 1;

            up_key = inner->keys[threshold - 1];
            if (child_sibling->temp_insert_key() > up_key)
            {
              assert(k >= threshold);
              to_insert = new_sibling;
              k -= threshold;
            }
          }
          new_sibling->temp_insert_key() = up_key;
        }

        if (k != -1)
        {
          for (int i = to_insert->num_keys; i > k; i--)
          {
            to_insert->keys[i] = to_insert->keys[i - 1];
            to_insert->children[i + 1] = to_insert->children[i];
          }

          to_insert->num_keys++;
          to_insert->keys[k] = child_sibling->temp_insert_key();
          to_insert->children[k] = child;
        }
        to_insert->children[k + 1] = child_sibling;
      }
      else
      {
      }
    }

    /**
     * check if we need to increase the depth of the tree
     */
    if (d == depth && new_sibling != nullptr)
    {
#if REMOTE
      using Alloc = r2::AllocatorMaster<>;
      auto temp = root;
      root = Alloc::get_thread_allocator()->alloc(sizeof(Node));
      ASSERT(root != nullptr);
      {
        Node *new_root = (Node *)root;
        new_root->keys[0] = up_key;
        new_root->children[0] = temp;
        new_root->children[1] = new_sibling;
        new_root->num_keys = 1;
      }
#else
      root = new Node(up_key, root, new_sibling);
#endif
      depth += 1;
    }
    return new_sibling;
  }

public:
  template <typename N>
  inline uint find_pos(KEY &key, N *n)
  {
    uint k = 0;
    // find the position in this node
    while (k < n->num_keys && key >= n->keys[k])//n->keys[k] < key)
    {
      k += 1;
    }

    return k;
  }
};

} // end namespace store

} // end namespace fstore
