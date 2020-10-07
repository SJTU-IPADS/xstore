#pragma once

#include "common.hpp"
#include "loader.hpp"

/*!
 * This file provides several wrapper over test benchmarks.
 */

namespace kv {

template<class Derived>
class BaseTester
{
public:
  inline u64 eval_func(u64 key)
  {
    return static_cast<Derived*>(this)->core_eval_func(key);
  }
};

class NullTester : public BaseTester<NullTester>
{
public:
  inline u64 core_eval_func(u64 key)
  {
    // just do nothing
    return 0;
  }
};

class BTreeTester : public BaseTester<BTreeTester>
{
public:
  BTreeTester(Tree* tree)
    : tree(tree)
  {}

  inline u64 core_eval_func(u64 key)
  {
    auto ptr = tree->get(key);
    ASSERT(ptr != nullptr);
    if (ptr == nullptr)
      return key;
    else
      return *ptr;
  }

private:
  Tree* tree;
};

class RAWInsertTester : public BaseTester<RAWInsertTester>
{
public:
  RAWInsertTester(Tree* t)
    : tree(t)
  {}

  inline u64 core_eval_func(u64 key)
  {
    auto ptr = tree->get_with_insert(key);
    ASSERT(ptr != nullptr);
    return *ptr;
  }

private:
  Tree* tree = nullptr;
};

class RTMInsertTester : public BaseTester<RTMInsertTester>
{
public:
  RTMInsertTester(Tree* t)
    : tree(t)
  {}

  inline u64 core_eval_func(u64 key)
  {
    auto ptr = tree->safe_get_with_insert(key);
    ASSERT(ptr != nullptr);
    return *ptr;
  }

private:
  Tree* tree;
};

class LearnedIndexTester : public BaseTester<LearnedIndexTester>
{
public:
  LearnedIndexTester(LearnedIdx* i)
    : index(i)
  {}

  inline u64 core_eval_func(u64 key) { return index->get(key); }

private:
  LearnedIdx* index = nullptr;
};

/*!
 * LearnedIdx has two parts:
 * - #1. predict the position.
 * - #2. find the tuple given the position.
 * The following class tests these functions separately.
 * - SmartCachetesterP only evaluates predict performance.
 * - SmartCachetester evaluates predicts + search.
 */
class SmartCacheTesterP : public BaseTester<SmartCacheTesterP>
{
public:
  SmartCacheTesterP(SC* cache)
    : sc(cache)
  {}

  inline u64 core_eval_func(u64 key)
  {
    auto p = sc->get_predict(key);
    return static_cast<u64>(p.pos);
  }

private:
  SC* sc = nullptr;
};

class SmartCacheTester : public BaseTester<SmartCacheTester>
{
public:
  SmartCacheTester(SC* cache)
    : sc(cache)
  {}

  inline u64 core_eval_func(u64 key)
  {
    auto ptr = sc->get(key);
    ASSERT(ptr != nullptr);
    return *ptr;
  }

private:
  SC* sc = nullptr;
};

class HybridStoreTester : public BaseTester<HybridStoreTester>
{
public:
  HybridStoreTester(HybridStore* hs)
    : store(hs)
  {}

  inline u64 core_eval_func(u64 key)
  {
    auto ptr = store->get(key);
    ASSERT(ptr != nullptr);
    return *ptr;
  }

private:
  HybridStore* store = nullptr;
};

}
