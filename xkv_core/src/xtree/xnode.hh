#pragma once

#include <limits>

#include "../../../xcomm/src/atomic_rw/wrapper_type.hh"
#include "./spin_lock.hh"

namespace xstore {

namespace xkv {
namespace xtree {

using namespace ::xstore::xcomm::rw;

template <usize N> struct __attribute__((packed)) XNodeKeys {
  static_assert(std::numeric_limits<u8>::max() >= N,
                "The number of keys is too large for XNode");
  u64 keys[N];
  u8 num_keys = 0;
  u32 incarnation = 0;

  // methods
  auto empty() -> bool { return num_keys == 0; }

  auto add_key(const u64 &key) -> bool {
    if (this->num_keys < N) {
      this->keys[this->num_keys] = key;
      this->num_keys += 1;
      return true;
    }
    return false;
  }

  auto get_key(const int &idx) -> u64 {
    if (idx < this->num_keys) {
      return this->keys[idx];
    }
    return {};
  }

  /*!
    memory offset of keys entry *idx*
  */
  usize key_offset(int idx) const {
    return offsetof(XNodeKeys<N>, keys) + sizeof(u64) * idx;
  }
};

/*!
  - N: max keys in this node
  - V: the value type
  FIXME: what if the V is a pointer? usually its trivially to adapt,
  but how to program it in a nice format ?
 */
template <usize N, typename V> struct __attribute__((packed)) XNode {

  CompactSpinLock lock;

  using NodeK = XNodeKeys<N>;

  // keys
  WrappedType<NodeK> keys;

  // values
  WrappedType<V> values[N];

  // next pointer
  XNode<N, V> *next = nullptr;

  // methods
  XNode() = default;

  auto get_key(const int &idx) -> Option<u64> {
    return keys.get_payload().get_key(idx);
  }

  /*!
    The start offset of keys
   */
  auto keys_start_offset() -> usize {
    return offsetof(XNode, keys);
  }
};
} // namespace xtree
} // namespace xkv
} // namespace xstore
