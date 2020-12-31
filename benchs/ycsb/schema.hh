#pragma once

#include "../../x_ml/src/lr/mod.hh"

#include "../../xcache/src/page_tt_iter.hh"
#include "../../xcache/src/rmi_2.hh"
#include "../../xcache/src/samplers/mod.hh"

/*!
  Defininations of the YCSB keytype, value type, B+Tree layout, etc
 */
namespace xstore {

using namespace xcache;
using namespace xml;
using namespace xkv::xtree;

template<usize N>
struct OpaqueVal
{
  char data[N];
};

// 8-byte value
const usize kNPageKey = 16;
using ValType = u64;
using DBTree = XTree<kNPageKey, XKey, ValType>;
using DBTreeIter = XTreeIter<kNPageKey, XKey, ValType>;

using XCache = LocalTwoRMI<LR, LR, XKey>;
using TrainIter = XCacheTreeIter<kNPageKey, XKey, ValType>;

template<typename Key>
using PS = PageSampler<kNPageKey, Key>;
// using PS = DefaultSample;

using DBTreeV = XTree<kNPageKey, XKey, FatPointer>;
using DBTreeIterV = XTreeIter<kNPageKey, XKey, FatPointer>;
using TrainIterV = XCacheTreeIter<kNPageKey, XKey, FatPointer>;

} // namespace xstore
