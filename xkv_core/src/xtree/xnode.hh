#pragma once

#include <limits>

#include "../../../xcomm/src/atomic_rw/unwrapper_type.hh"
#include "../../../xcomm/src/atomic_rw/wrapper_type.hh"

#include "./spin_lock.hh"

#include "./xkeys.hh"

namespace xstore {

namespace xkv {

namespace xtree {

using namespace ::xstore::xcomm::rw;

// TODO: current not implemented a sorted node key, but it is trivial
const bool kKeysSorted = false;

#ifndef SIZEOF_TYPE
#define SIZEOF_TYPE(X) (((X*)0) + 1)
#endif

#define XNODE_KEYS_ATOMIC 0

template<usize N, typename V>
union __attribute__((packed)) Values
{
  V inplace[N];
  WrappedType<V> wrapped[N];

  Values() {}

  static auto offset_inplace(const usize& idx) -> usize
  {
    return offsetof(Values, inplace[idx]);
  }

  static auto offset_wrapped(const usize& idx) -> usize
  {
    return offsetof(Values, wrapped[idx]);
  }
};

/*!
  - N: max keys in this node
  - V: the value type
  FIXME: what if the V is a pointer? usually its trivially to adapt,
  but how to program it in a nice format ?
 */
template<usize N, typename K, typename V>
struct __attribute__((packed)) XNode
{

  using NodeK = XNodeKeys<N, K>;

  // keys
#if XNODE_KEYS_ATOMIC
  WrappedType<NodeK> keys;
#else
  NodeK keys;
#endif

  // values
  // if the size of value is smaller, we directly store it

  // FIXME: how to define it statically?
  // UWrappedType<V> values[N];
  Values<N, V> values;

  // next pointer
  XNode<N, K, V>* next = nullptr;

  CompactSpinLock lock;

  // methods
  XNode() = default;

  auto keys_ptr() -> NodeK*
  {
#if XNODE_KEYS_ATOMIC
    return keys.get_payload_ptr();
#else
    return &(this->keys);
#endif
  }

  auto num_keys() -> usize { return this->keys_ptr()->num_keys(); }

  auto get_incarnation() -> IncarType { return this->keys_ptr()->incarnation; }

  auto get_key(const int& idx) -> K { return this->keys_ptr()->get_key(idx); }

  auto get_value(const int& idx) -> ::r2::Option<V>
  {
    // TODO: not check idx
    if (this->keys_ptr()->get_key(idx) != K(kInvalidKey)) {
      return get_value_raw(idx);
    }
    return {};
  }

  // unsafe code, not check the idx and whether the idx's key is valid
  auto get_value_raw(const int& idx) -> V
  {
    if (sizeof(V) <= sizeof(u64)) {
      return values.inplace[idx];
    } else {
      return *(values.wrapped[idx].get_payload_ptr());
    }
  }

  /*!
    The start offset of keys
   */
  static auto keys_start_offset() -> usize { return offsetof(XNode, keys); }

  /*!
    The start offset of values
   */
  static auto value_start_offset() -> usize { return offsetof(XNode, values); }

  static auto inplace_value_end_offset() -> usize
  {
    return value_start_offset() + sizeof(V) * N;
  }

  static auto value_offset(const usize& idx) -> usize
  {
    if (sizeof(V) <= sizeof(u64)) {
      return value_start_offset() + Values<N, V>::offset_inplace(idx);
    } else {
      return value_start_offset() + Values<N, V>::offset_wrapped(idx);
    }
  }

  /*!
    Query method
   */
  auto search(const K& key) -> ::r2::Option<u8>
  {
    return this->keys_ptr()->search(key);
  }

  auto insert(const K& key, const V& v, XNode<N, K, V>* candidate) -> bool
  {
    // this->lock.lock();
    auto ret = this->raw_insert(key, v, candidate);
    // this->lock.unlock();
    return ret;
  }

  void print()
  {
    for (uint i = 0; i < N; ++i) {
      if (this->get_key(i) != K(kInvalidKey)) {
        LOG(4) << "keys: #" << i << " " << this->get_key(i);
      }
    }
  }

  /*!
    inplace update value
    update value at *idx*
  */
  auto value_update(const int& idx, const V& v)
  {
    if (sizeof(V) <= sizeof(u64)) {
      // can use `store` for atomic update
      this->values.inplace[idx] = v;
    } else {
      // use the wrappedtype
      this->values.wrapped[idx].reset(v);
    }
  }

  /*!
    Core insert function
    raw means that we donot hold the lock.
    Warning: we assume that candidate is **exclusively owned** by this
    insertion thread, and we will use raw_insert on candidate \ret: true ->
    this node has splitted, the splitted node is stored in the candidate
   */
  auto raw_insert(const K& key, const V& v, XNode<N, K, V>* candidate) -> bool
  { // lock the node for atomicity
    bool ret = false;
    auto idx = this->keys_ptr()->add_key(key);
    if (idx) {
      // in-place update
      // this->values[idx.value()].reset(v);
      this->value_update(idx.value(), v);
    } else {
      // split
      ASSERT(candidate != nullptr)
        << "split at the node:" << this << " " << candidate;
      /*
        Split is more tricky in XTree, because keys in this node can be
        un-sorted. Plan #1: select the medium key, and split Plan #2: first sort
        the keys, and then split Currently, we use plan #1 due to simplicity
       */

      // 1. increment the incarnation
      this->keys_ptr()->incarnation += 1;
      r2::compile_fence();

      // 2. move the pivot key
      auto pivot_key_idx = this->keys_ptr()->find_median_key().value();
      auto pivot_key = this->keys_ptr()->get_key(pivot_key_idx);

      // should not split
      auto r = candidate->raw_insert(
        pivot_key, this->get_value_raw(pivot_key_idx), nullptr);
      ASSERT(r == false);
      this->keys_ptr()->clear(pivot_key_idx);

      // 3. then, move other keys
      for (uint i = 0; i < N; ++i) {
        auto k = this->get_key(i);
        if (k != K(kInvalidKey) && k > pivot_key) {
          // insert
          // auto r =
          // candidate->raw_insert(k, *(this->values[i].get_payload_ptr()),
          // nullptr);
          auto r = candidate->raw_insert(k, this->get_value_raw(i), nullptr);
          ASSERT(r == false);
          this->keys_ptr()->clear(i);
        }
      }

      r2::compile_fence();
      // 4. insert the newly inserted keys
      if (key > pivot_key) {
        candidate->raw_insert(key, v, nullptr);
      } else {
        this->raw_insert(key, v, nullptr); // should have a space
      }
      // ASSERT(this->num_keys() + candidate->num_keys() == N + 1);

      // re-set the next pointer
      candidate->next = this->next;
      this->next = candidate;

      ret = true; // notify the insertion process the splition
    }
    return ret;
  }

  friend std::ostream& operator<<(std::ostream& out, const XNode& mod)
  {
    for (uint i = 0; i < N; ++i) {
      if (mod.keys.get_key(i) != K(kInvalidKey)) {
        out << mod.keys.get_key(i) << ", ";
      }
    }
    return out;
  }
};
} // namespace xtree
} // namespace xkv
} // namespace xstore
