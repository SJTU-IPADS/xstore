#pragma once

#include <map>

#include "../../deps/r2/src/common.hh"
#include "../../xcomm/src/atomic_rw/wrapper_type.hh"
#include "./kv_trait.hh"

// Memblock, which abstract away a raw pointer
#include "../../deps/r2/src/mem_block.hh"

/*!
  XArray provides a sorted array as the KV
 */
namespace xstore {

namespace xkv {

using namespace ::xstore::xcomm::rw;

template <typename KeyType, typename V>
struct XArray : public KVTrait<XArray<KeyType, V>, KeyType, V> {

  using VType = WrappedType<V>;

  MemBlock key_array;
  MemBlock val_array;

  std::map<KeyType, usize> index;

  // unsafe pointer
  // these two pointers would point to the address in key_array and val_array
  KeyType *key_ptr = nullptr;
  WrappedType<V> *val_ptr = nullptr;

  usize size = 0;

  XArray(const MemBlock &key_mem, const MemBlock &val_mem)
      : key_array(key_mem), val_array(val_mem),
        key_ptr(reinterpret_cast<KeyType *>(key_mem.mem_ptr)),
        val_ptr(reinterpret_cast<WrappedType<V> *>(val_mem.mem_ptr)) {}

  explicit XArray(const usize &max_num_k)
      : XArray(MemBlock(new char[max_num_k * sizeof(KeyType)],
                        max_num_k * sizeof(KeyType)),
               MemBlock(new char[max_num_k * sizeof(WrappedType<V>)],
                        max_num_k * sizeof(WrappedType<V>))) {}

  /*!
    \ret true if insertion success, false if failure
    insertion could be reject due to two reasons:
    1. there are no avaliable memory, either because key_mem is out of space or
    val_mem is out of space
    2. the k is smaller than the current key, because the array must be sorted
   */
  auto insert_w_index(const KeyType &k, const V &v, bool add_idx = false)
      -> bool {
    if (size != 0) {
      // check prev key
      if (this->keys_at(size - 1).value() >= k) {
        return false;
      }
    }
    // check whether there is free memory
    if ((this->size + 1) * sizeof(u64) > this->key_array.sz) {
      return false;
    }

    if ((this->size + 1) * sizeof(WrappedType<V>) > this->val_array.sz) {
      return false;
    }

    key_ptr[this->size] = k;
    val_ptr[this->size].reset(v);

    if (add_idx) {
      this->index.insert(std::make_pair(k, this->size));
    }
    this->size += 1;

    return true;
  }

  auto keys_at(const int &idx) -> ::r2::Option<KeyType> {
    if (likely(idx >= 0 && idx < size)) {
      return key_ptr[idx];
    }
    return {};
  }

  auto vals_at(const int &idx) -> ::r2::Option<V> {
    if (likely(idx >= 0 && idx < size)) {
      return *(val_ptr[idx].get_payload_ptr());
    }
    return {};
  }

  /*!
    Impl the KVTrait
   */

  auto get_impl(const KeyType &k) -> ::r2::Option<V> {
    auto p = this->pos(k);
    if (p) {
      return this->vals_at(p.value());
    }
    return {};
  }

  auto pos(const KeyType &k) -> ::r2::Option<usize> {
    auto it = this->index.find(k);
    if (it != this->index.end()) {
      return it->second;
    }
    return {};
  }

  auto insert_impl(const KeyType &k, const V &v) {
    return this->insert_w_index(k, v, true);
  }
};

} // namespace xkv
} // namespace xstore
