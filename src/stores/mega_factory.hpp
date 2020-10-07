#pragma once

#include "../utils/all.hpp"
#include "../pager.hpp"

#include "tree_iters.hpp"

namespace fstore {

namespace store {

/*!
  Given a NaiveTree, generates its address distribution, aka
    k0, mega addr 0; k1, mega addr 1, ...
  For these mega address, nega addr 0 is strictly smaller than mega addr2
 */
template <typename K,typename V,template <class> class P = BlockPager>
class MegaFactory {
  typedef BMegaStream<K,V,P>  BIdStream;
 public:
  static ::fstore::utils::DataMap<K,u64> produce(MegaPager &pp,NaiveTree<K,V,P> &t, const K &min_key) {
    ::fstore::utils::DataMap<K,u64> res("origin");

    BIdStream it(t,min_key,&pp);
    for(it.begin();it.valid();it.next()) {
      // really insert the key into the cdf
      res.insert(it.key(),it.value());
    }

    return res;
  }
};

} // end namespace store

} // end namespace fstore
