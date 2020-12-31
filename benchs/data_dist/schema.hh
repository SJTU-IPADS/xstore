#pragma once

#define USE_TNN 0

#if USE_TNN
#include "../../x_ml/src/nn/nn_tiny.hh"
#else
#include "../../x_ml/src/nn/mod.hh"

#include "./models/log_nn.hh"
#include "./models/map_nn.hh"
#endif

#include "../../x_ml/src/lr/mod.hh"
#include "../../x_ml/src/mv_lr.hh"
#include "./server/ar_key.hh"
#include "./server/map_key.hh"
#include "./server/tpcc_key.hh"

#include "../../xcache/src/page_tt_iter.hh"
#include "../../xcache/src/rmi_2.hh"
#include "../../xcache/src/samplers/mod.hh"

#include "../../../xkv_core/src/xtree/sorted_iter.hh"
/*!
  Defininations of the YCSB keytype, value type, B+Tree layout, etc
 */
namespace xstore {

using namespace xcache;
using namespace xml;
using namespace xkv::xtree;

//#if USE_MV
// using log normal dataset
#define USE_LOG 0
#define USE_AR 0
#define USE_TPCC 0

#ifndef USE_MAP
#define USE_MAP 1
#endif

#if USE_LOG
using KK = XKey;
const usize D = 1;
#endif

#if USE_AR
using KK = ARKey;
const usize D = 2;
#endif

#if USE_TPCC
using KK = TPCCKey;
const usize D = 4;
#endif

#if USE_MAP
using KK = MapKey;
const usize D = 2;
#endif

template<usize N>
struct OpaqueVal
{
  char data[N];
};

// 8-byte value
const usize kNPageKey = 16;
using ValType = u64;
using DBTree = XTree<kNPageKey, KK, ValType>;
using DBTreeIter = XTreeIter<kNPageKey, KK, ValType>;
using DBTreeSIter = XTreeSIter<kNPageKey, KK, ValType>;

template<typename Key>
using MMLR = MvLR<D, Key>;

#if USE_MV
template<typename Key>
using SML = MMLR<Key>;
#endif

#if !USE_TNN
template<typename Key>
using TNN = NN<LogNet, DualLogNet, D, Key>;

template<typename Key>
using MNN = NN<MapNet, DualMapNet, D, Key>;
#else
template<typename Key>
using TNN = XNN<Key>;
#endif

#if USE_NN
template<typename Key>
using SML = MNN<Key>;
#endif

#if !USE_NN && !USE_MV
template<typename Key>
using SML = LR<Key>;
#endif

// using XCache = LocalTwoRMI<TNN, SML, KK>;
using XCache = LocalTwoRMI<LR, SML, KK>;

// using XCache = LocalTwoRMI<MMLR, SML, KK>;

using TrainIter = XCacheTreeIter<kNPageKey, KK, ValType>;

template<typename Key>
using PS = PageSampler<kNPageKey, Key>;
// using PS = DefaultSample;

using DBTreeV = XTree<kNPageKey, KK, FatPointer>;
using DBTreeIterV = XTreeIter<kNPageKey, KK, FatPointer>;
using TrainIterV = XCacheTreeIter<kNPageKey, KK, FatPointer>;

} // namespace xstore
