#pragma once

#include "common.hpp"

#include "custom_trait.hpp"
//#include "smart_cache.hpp"
#include "stores/naive_tree.hpp"

namespace fstore {

using namespace store;

/*!
  Since we use value-in-index design, so the trait (keytype,valuetype of DB must
  be pre-defined). This file provides the definiation of such traits.
 */
using KeyType = u64;
using ValType = OpaqueData<8>;
  //using ValType = OpaqueData<8>;
  //using ValType = OpaqueData<64>;
// using ValType = u64;

// TODO: replace with masstree, or others
// further, replace with fixed sized value
using Tree = NaiveTree<KeyType, ValType, BlockPager>;
using Leaf = LeafNode<KeyType, ValType, BlockPager>;
// using SC = SCache<KeyType, ValType, Leaf, BlockPager>;
using Inner = InnerNode<KeyType>;

using TestLidx = LearnedRangeIndexSingleKey<ValType,char>;

} // fstore
