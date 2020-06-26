#pragma once

#include <functional>

#include "../../xkv_core/src/xtree/iter.hh"
#include "./logic_addr.hh"
#include "./translation_table.hh"

namespace xstore {

namespace xcache {

using namespace xstore::xkv::xtree;
using namespace xstore::xkv;

using TTEntryType = u64;
using XCacheTT = TT<TTEntryType>;

/*!
  An iteraetor which will add TT entries to to the TT when iterating through
  nodes.

  The tests are in ../tests/test_rmi_tree.cc
 */
template <usize N, typename V>
struct XCacheTreeIter : public KeyIterTrait<XCacheTreeIter<N, V>, XTree<N, V>> {
  // a user must provide a function to generate TT entries upon iterating
  // through a new node
  // FIXME: should we change the fn to fn (XNode<N,V> *, XNode<N,V> &snap) -> TTEntryType ?
  // Since the training may be on the snapshot (which is a local copy)
  using TTEntryGenF = std::function<TTEntryType(XNode<N, V> *)>;

  TTEntryGenF gen_f = [](XNode<N, V> *n_ptr) -> TTEntryType {
    return reinterpret_cast<TTEntryType>(n_ptr);
  };

  XNode<N, V> *prev_node = nullptr;
  XTreeIter<N, V> core_iter; // basically, this is a wrapper of the core_iter
  XCacheTT *tt = nullptr;
  u64 logic_page_id = 0;

  using Self = XCacheTreeIter<N, V>;

  static auto from_impl(XTree<N, V> &kv) -> Self { return Self(kv, nullptr); }

  static auto from_tt(XTree<N, V> &kv, TT<TTEntryType> *tt) -> Self {
    return Self(kv, tt);
  }

  XCacheTreeIter(XTree<N, V> &kv, TT<TTEntryType> *tt)
      : core_iter(kv), tt(tt) {}

  // impl traits
  auto begin_impl() {
    this->prev_node = nullptr;
    this->core_iter.begin();
    this->reset_old_ptr();
  }

  auto next_impl() {
    this->core_iter.next();
    this->reset_old_ptr();
  }

  auto has_next_impl() -> bool { return this->core_iter.has_next(); }

  auto seek_impl(const KeyType &k, XTree<N, V> &kv) {
    this->prev_node = nullptr;
    this->core_iter.seek(k, kv);
    this->reset_old_ptr();
  }

  auto cur_key_impl() -> KeyType { return this->core_iter.cur_key(); }

  auto opaque_val_impl() -> u64 {
    return LogicAddr::encode_logic_addr(this->logic_page_id,
                                        this->core_iter.idx);
  }

  auto reset_old_ptr() {
    if (core_iter.has_next()) {
      if (core_iter.cur_node_ptr != this->prev_node) {
        this->prev_node = core_iter.cur_node_ptr;
        // add to TT
        tt->add(this->gen_f(this->prev_node));
        this->logic_page_id += 1;
      }
    }
  }
};
} // namespace xcache
} // namespace xstore
