#pragma once

#include "./iter_trait.hh"
#include "./xarray.hh"

namespace xstore {

namespace xkv {

/*!
  Implements the key iterate trait to XArray
 */
template <typename KeyType, typename V>
struct ArrayIter : public KeyIterTrait<ArrayIter<KeyType,V>, XArray<KeyType,V>, KeyType> {

  // members
  using Self = ArrayIter<KeyType,V>;
  usize cur_idx = 0;
  XArray<KeyType, V> *kv;

  static auto from_impl(XArray<KeyType, V> &kv) -> Self { return Self(kv); }

  ArrayIter(XArray<KeyType,V> &kv) : cur_idx(0), kv(&kv) {}

  auto begin_impl() { this->cur_idx = 0; }

  auto next_impl() { this->cur_idx += 1; }

  auto has_next_impl() -> bool { return this->cur_idx < kv->size; }

  auto cur_key_impl() -> KeyType { return kv->keys_at(this->cur_idx).value(); }

  auto opaque_val_impl() -> u64 { return cur_idx; }

  auto seek_impl(const KeyType &k, XArray<KeyType, V> &) {
    auto pos = kv->pos(k);
    if (pos) {
      this->cur_idx = pos.value();
    } else {
      // invalid
      pos = kv->size;
    }
  }
};
} // namespace xkv

} // namespace xstore
