#pragma once

#include "../common.hpp"

#include "pager.hpp"

#include <stddef.h>

namespace fstore {

namespace store {

const uint IM = 16;
const uint IN = 16;

template<typename KEY = u64,
         typename VAL = u64,
         template<class> class P = BlockPager>
struct __attribute__((aligned(CACHE_LINE_SZ))) __attribute__((packed)) LeafNode
{

  static_assert(std::numeric_limits<u8>::max() >= std::max(IM, IN), "");
  /**
   * XD: Shall we maintain the { key + value } structure, or seperately?
   * Seperate arrays are better: because scan keys has better cache locality
   * (and less network transfered).
   */
  KEY keys[IM];
  u8 num_keys = 0;
  u32 seq = 1;
  LeafNode* right = nullptr;

  VAL values[IM];

  LeafNode* left = nullptr;


  static u64 key_offset(u64 offset)
  {
    using leaf = LeafNode<KEY, VAL, P>;
    return offsetof(leaf, keys) + sizeof(KEY) * offset;
  }

  static u64 value_offset(u64 offset)
  {
    using leaf = LeafNode<KEY, VAL, P>;
    return offsetof(leaf, values) + sizeof(VAL) * offset;
  }

  static u64 seq_offset() {
    using leaf = LeafNode<KEY, VAL, P>;
    return offsetof(leaf,seq);
  }

  KEY start_key() const { return keys[0]; }

  KEY end_key() const { return keys[num_keys - 1]; }

  inline int get_free_slot(KEY key) const
  {
    int slot = 0;
    while (slot < num_keys) {
      if (key >= keys[slot])
        break;
      slot++;
    }
    return slot;
  }

  /*!
   * Insert a val to this leaf node, split another leaf node if overflow
   */
  inline LeafNode<KEY, VAL,P>* insert(KEY key, VAL** res)
  {

    LeafNode<KEY, VAL,P>* new_sibling = nullptr;
    uint k = 0;
    // find the position in this node
    while (k < num_keys && keys[k] < key) {
      //LOG(4) << "check: " << keys[k] << " " << key;
      k += 1;
    }

    // the value is original here, return
    if (k < num_keys && key == keys[k]) {
      //LOG(4) << "insert find one exsist";
      *res = &values[k];
      return nullptr;
    }
    //LOG(4) << "insert failed to find one " << this << " ; with k,num " << k << " " << (int)num_keys;
    auto to_insert = this;

    if (num_keys == IM) {
      // allocating new ones
      new_sibling = P<LeafNode<KEY, VAL,P>>::allocate_one();
      ASSERT(new_sibling != nullptr);
      new_sibling->seq = 1;
      ASSERT(new_sibling != nullptr);
      // new_sibling = new LeafNode<KEY,VAL>;
      ASSERT(new_sibling != nullptr);

      //if (right == nullptr && k == num_keys) {
      if (0) {
        new_sibling->num_keys = 0;
        k = 0;
        to_insert = new_sibling;
      } else {
        unsigned threshold = (IM + 1) / 2;

        new_sibling->num_keys = num_keys - threshold;

        for (uint j = 0; j < new_sibling->num_keys; ++j) {
          new_sibling->keys[j] = keys[threshold + j];
          new_sibling->values[j] = values[threshold + j];
        }
        num_keys = threshold;
        r2::compile_fence();

        if (k >= threshold) {
          k -= threshold;
          to_insert = new_sibling;
        }
      }
      r2::compile_fence();
      this->seq += 1;
      /**
       * swap the pointer (B+ links)
       */
      if (right)
        right->left = new_sibling;
      new_sibling->right = right;
      new_sibling->left = this;
      right = new_sibling;
    }

    for (int j = to_insert->num_keys; j > k; j--) {
      to_insert->keys[j] = to_insert->keys[j - 1];
      to_insert->values[j] = to_insert->values[j - 1];
    }

    to_insert->num_keys += 1;
    to_insert->keys[k] = key;
    *res = &(to_insert->values[k]);
    return new_sibling;
  }

  inline VAL delete_slot(uint slot)
  {
    VAL res = values[slot];
    num_keys -= 1;

    for (uint i = slot + 1; i <= num_keys; ++i) {
      keys[i - 1] = keys[i];
      values[i - 1] = values[i];
    }
    return res;
  }

  std::string to_str() const
  {
    std::ostringstream oss;
    oss << "(node entries " << num_keys << ") :";
    for (uint i = 0; i < num_keys; ++i) {
      oss << "[k: " << keys[i] << ", v: " << values[i] << " ]";
    }
    oss << std::endl;
    return oss.str();
  }

  static constexpr uint page_entry() { return IM; }

  void sanity_check()
  {
    // ASSERT(num_keys > 0)  << "node has zero keys";
    ASSERT(num_keys > 0 && num_keys <= IM)
      << "sanity check node failure, too many num keys: " << (usize)num_keys;
  }

  bool sanity_check_me() {
    if (num_keys == 0 || num_keys > IM) {
      return false;
    }
    return true;
  }
};

template<typename KEY = u64>
struct __attribute__((aligned(CACHE_LINE_SZ))) InnerNode
{
  InnerNode()
  {
    for (uint i = 0; i < IN + 1; ++i)
      children[i] = nullptr;
  }

  InnerNode(KEY& k, void* c0, void* c1)
    : num_keys(1)
  {
    children[0] = c0;
    children[1] = c1;
    keys[0] = k;
  }

  inline void copy_to(InnerNode* node, int num) const
  {
    assert(num < num_keys);
    node->num_keys = num_keys - num;
    for (uint i = 0; i < node->num_keys; ++i) {
      node->keys[i] = keys[num + i];
      node->children[i] = children[num + i];
    }
    node->children[node->num_keys] = children[num_keys];
  }

  std::string to_str() const
  {
    std::ostringstream oss;
    oss << "(inner entries " << num_keys << ") :";
    for (uint i = 0; i < num_keys; ++i)
      oss << " | " << keys[i];
    oss << "|" << std::endl;
    return oss.str();
  }

  unsigned num_keys = 0;
  KEY keys[IN + 1];
  void* children[IN + 1];

  inline int get_free_slot(KEY key) const
  {
    int slot = 0;
    while (slot < num_keys) {
      if (key >= keys[slot])
        break;
      slot++;
    }
    return slot;
  }

  inline KEY& temp_insert_key() { return keys[IN]; }
};

} // end namespace store

} // end namespace fstore
