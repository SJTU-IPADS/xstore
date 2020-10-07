#pragma once

#include "mousika/learned_index.h"

#include "datastream/stream.hpp"

#include <algorithm>
#include <limits>

namespace fstore {

/*!
  The index factory create learned index based on set of keys/(and their corresponding addresses).
 */
template <typename K, typename V>
class LIndexFactory {
  typedef LearnedRangeIndexSingleKey<K,V> Learned_index_t;
 public:
  /*!
    Create a learned index, assuming the addresses of all keys are **sort array**.
    \param keys: the keys to be located in the sorted array
    \param idx:  the results index
    \note: the vector of keys will be sorted after the call.
    \note: this method is mostly deprecated.
   */
  static void create_naive_index(std::vector<K> &keys,Learned_index_t &idx) {
    // first we sort the keys, which is important for learned index
    std::sort(keys.begin(),keys.end());
    assert(check_vector_sorted(keys));

    // then we insert these keys into the learned index
    for(auto k : keys) {
      assert(sanity_check_key(k));
      idx.insert(k,0); // we donot need the value, so just uses the 0 for dummy
    }
    // the learning happends here
    idx.finish_insert();
  }

  /*!
    create index from the data stream, use the default *sorted array*
   */
  static void create_idx_from_stream(datastream::StreamIterator<K,V> *iter,Learned_index_t &idx) {
    for(iter->begin();iter->valid();iter->next()) {
      assert(sanity_check_key(iter->key()));
      idx.insert(iter->key(),0);
    }
    idx.finish_insert();
  }

  /*!
    create index from the data stream, but instead, use the address provided by the stream
  */
  static void create_idx_from_lstream(datastream::StreamIterator<K,learned_addr_t> *iter,
                                      Learned_index_t &idx) {
    u32 count = 0;
    for(iter->begin();iter->valid();iter->next()) {

      assert(sanity_check_key(iter->key()));
      idx.insert(iter->key(),0,iter->value());
      //idx.insert(iter->key(),0,count);
      //count += 64;
    }
    idx.finish_insert();
  }

  static void create_idx_from_pstream(datastream::StreamIterator<u64,mega_id_t> *iter,
                                      Learned_index_t &idx) {
    u64 min_key = 0;
    for(iter->begin();iter->valid();iter->next()) {
      assert(sanity_check_key(iter->key()));
      auto key = iter->key();
      if (key < min_key)
        assert(false);
      min_key = key;
#if 1
      idx.insert(key,0,iter->value());
#else
      idx.insert(key,0);
#endif
    }
    idx.finish_insert();
  }

  /*!
    The learned index uses *double* to train the model, so we need to ensure
    that the key is smaller than the maxinum limit of double.
   */
  static bool sanity_check_key(K key) {
    return key < std::numeric_limits<double>::max();
  }

  /*!
    Check whether the vector is sorted.
   */
  static bool check_vector_sorted(const std::vector<K> &keys) {
    for(uint i = 1;i < keys.size();++i)
      if(keys[i] < keys[i-1]) return false;
    return true;
  }

}; // end class

} // end namespace fstore
