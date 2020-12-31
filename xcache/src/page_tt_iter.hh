#pragma once

#include <functional>

#include "../../xkv_core/src/xtree/sorted_iter.hh"
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
template<usize N, typename K, typename V>
struct XCacheTreeIter
  : public KeyIterTrait<XCacheTreeIter<N, K, V>, XTree<N, K, V>, K>
{
  // a user must provide a function to generate TT entries upon iterating
  // through a new node
  // FIXME: should we change the fn to fn (XNode<N,V> *, XNode<N,V> &snap) ->
  // TTEntryType ? Since the training may be on the snapshot (which is a local
  // copy)
  using TTEntryGenF = std::function<TTEntryType(XNode<N, K, V>*)>;

  TTEntryGenF gen_f = [](XNode<N, K, V>* n_ptr) -> TTEntryType {
    return reinterpret_cast<TTEntryType>(n_ptr);
  };

  XNode<N, K, V>* prev_node = nullptr;

  // FIXME: can the sorted iter be not sorted?
  XTreeSIter<N, K, V>
    core_iter; // basically, this is a wrapper of the core_iter
  XCacheTT* tt = nullptr;
  u64 logic_page_id = 0;

  using Self = XCacheTreeIter<N, K, V>;

  static auto from_impl(XTree<N, K, V>& kv) -> Self
  {
    return Self(kv, nullptr);
  }

  static auto from_tt(XTree<N, K, V>& kv, TT<TTEntryType>* tt) -> Self
  {
    return Self(kv, tt);
  }

  XCacheTreeIter(XTree<N, K, V>& kv, TT<TTEntryType>* tt)
    : core_iter(kv)
    , tt(tt)
  {}

  // impl traits
  auto begin_impl()
  {
    this->prev_node = nullptr;
    this->core_iter.begin();
    this->reset_old_ptr();
  }

  auto next_impl()
  {
    this->core_iter.next();
    this->reset_old_ptr();
  }

  auto has_next_impl() -> bool { return this->core_iter.has_next(); }

  auto seek_impl(const K& k, XTree<N, K, V>& kv)
  {
    this->logic_page_id = 0;
    this->prev_node = nullptr;
    this->core_iter.seek(k, kv);
    this->reset_old_ptr();
  }

  auto cur_key_impl() -> K { return this->core_iter.cur_key(); }

  auto opaque_val_impl() -> u64
  {
    // return LogicAddr::encode_logic_addr(this->logic_page_id,
    // this->core_iter.idx);
    ASSERT(this->logic_page_id > 0);
    return LogicAddr::encode_logic_addr<N>(this->logic_page_id - 1,
                                           this->core_iter.idx);
  }

  auto reset_old_ptr()
  {
    if (core_iter.has_next()) {
      if (core_iter.cur_node_ptr != this->prev_node) {
        // add to TT
        // tt->add(this->gen_f(core_iter.cur_node_ptr));
        tt->add_w_incar(this->gen_f(core_iter.cur_node_ptr),
                        core_iter.cur_node_ptr->get_incarnation());
        this->prev_node = core_iter.cur_node_ptr;
        this->logic_page_id += 1;
      }
    }
  }
};

template<usize N>
inline std::pair<int, int>
page_update_func(const u64& label,
                 const u64& predict,
                 const int& cur_min,
                 const int& cur_max)
{
  // no need to predict even they are different
  if (LogicAddr::decode_logic_id<N>(label) ==
      LogicAddr::decode_logic_id<N>(predict)) {
    return std::make_pair(cur_min, cur_max);
  }

  auto min_error =
    std::min(static_cast<int>(cur_min), static_cast<int>(label - predict));
  auto max_error =
    std::max(static_cast<int>(cur_max), static_cast<int>(label - predict));
  return std::make_pair(min_error, max_error);
}

} // namespace xcache
} // namespace xstore
