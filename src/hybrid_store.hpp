#pragma once

#include "smart_cache.hpp"
#include "page_fetch.hpp"

namespace fstore {

/*!
  \param: T, the data structure, for example, B+tree
  \param: SC: a SmartCache instance
 */
template <class T, class SC, typename K,typename V>
class HybridLocalStore {
 public:
  HybridLocalStore(T *tee,SC *sc,int threshold = 16)
      : main_store(tee),cache(sc), threshold(threshold) {
  }

  V *get(const K &k) {
    auto predict = cache->get_predict(k);
    assert(predict.start <= predict.end);
    if(predict.end - predict.start > threshold)
      return main_store->get(k);
    return cache->get_from_predicts(k,predict);
  }

 private:
  T  *main_store = nullptr;
  SC *cache      = nullptr;
  int threshold  = 1;
};

} // end namespace fstore
