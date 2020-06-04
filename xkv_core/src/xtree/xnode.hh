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

  auto full() -> bool { return num_keys == N; }

  /*!
    \ret: the index installed
   */
  auto add_key(const u64 &key) -> Option<u8> {
    if (this->num_keys < N) {
      this->keys[this->num_keys] = key;
      this->num_keys += 1;
      return this->num_keys - 1;
    }
    return {};
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
  auto keys_start_offset() -> usize { return offsetof(XNode, keys); }

  /*!
    The start offset of values
   */
  auto value_start_offset() -> usize { return offsetof(XNode, values); }

  /*!
    Core insert function
    \ret: true -> this node has splitted, the splitted node is stored in the candidate
   */
  auto insert(const u64 &key, const V &v, XNode<N, V> *candidate) -> bool {
    // lock the node for atomicity
    lock.lock();

    bool ret = false;
    if (this->keys.get_payload().full()) {
      // split
      /*
        Split is more tricky in XTree, because keys in this node can be un-sorted.
        Plan #1: select the medium key, and split
        Plan #2: first sort the keys, and then split
        Currently, we use plan #1 due to simplicity
       */
      ret = true;


    } else {
      auto idx = this->keys.get_payload().add_key(key);
      this->values[idx].reset(v);
    }

    lock.unlock();
    return ret;
  }
};
} // namespace xtree
} // namespace xkv
} // namespace xstore
