#pragma once

#include "loader.hpp"

namespace kv {
/*!
    The sample stream, which sample non-exsisting keys
    The char is a dummy placeholder
 */
class SampleStream : public datastream::StreamIterator<u64, char>
{
public:
  SampleStream(Tree& t, u64 min_key, u64 max_key)
    : start_page(t.find_leaf_page(min_key))
    , cur_page(start_page)
    , min_key(min_key)
    , max_key(max_key)
  {}

  void begin() override { cur_page = start_page; }

  bool valid() override
  {
    return cur_page != nullptr && cur_idx < cur_page->num_keys;
  }

  void next() override
  {
    if (cur_idx == cur_page->num_keys - 1) {
      cur_page = cur_page->right;
      cur_idx = 0;
    } else
      cur_idx += 1;
  }

  u64 key() override
  {
    auto prev_key = min_key;
    auto next_key = max_key;
    if (cur_idx == 0) {
      if (cur_page->left != nullptr)
        prev_key = cur_page->left->end_key();
      next_key = cur_page->keys[cur_idx];
    } else if (cur_idx == cur_page->num_keys - 1) {
      if (cur_page->right != nullptr)
        next_key = cur_page->right->start_key();
      prev_key = cur_page->keys[cur_idx];
    } else {
      prev_key = cur_page->keys[cur_idx];
      next_key = cur_page->keys[cur_idx + 1];
    }
    return prev_key + (next_key - prev_key) / 2;
  }

  /*!
    a dummy value
   */
  char value() override { return '0'; }

private:
  Leaf* start_page = nullptr;
  Leaf* cur_page = nullptr;
  u64 min_key;
  u64 max_key;
  usize cur_idx = 0;
};
} // namespacekv
