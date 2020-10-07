#pragma once

#include "pager.hpp"

namespace fstore {

/*!
  a helper class to read value given a set of page_ids
 */
template <typename K,typename V,typename PAGE_TYPE,template <class> class PAGEAlloc>
class PageFetcher {
 public:
  /*!
    If we know the exact position of key in one page:
    then we can directly fetch it.
   */
  inline static V* exact_fetch(page_id_t pid, uint pos) {
    PAGE_TYPE *p = PAGEAlloc<PAGE_TYPE>::get_page_by_id(pid);
    return &(p->values[pos]);
  }

  inline static V *exact_fetch(PAGE_TYPE *page,const K &key) {
    for(uint i = 0;i < page->num_keys;++i)
      if(page->keys[i] == key)
        return &(page->values[i]);
    return nullptr;
  }

  inline static Option<int> lookup(PAGE_TYPE *page,const K &key) {
    for(int i = 0;i < page->num_keys;++i)
      if(page->keys[i] == key)
        return Option<int>(i);
    return {};
  }


  inline static V* fetch_from_one_page(const K &k,page_id_t pid) {
    PAGE_TYPE *p = PAGEAlloc<PAGE_TYPE>::get_page_by_id(pid);
    if(unlikely(p == nullptr))
      return nullptr;
    return exact_fetch(p,k);
  }

  inline static V*fetch_from_two_page(const K &k, page_id_t p0, page_id_t p1) {
    return fetch_from_one_page(k, p0) || fetch_from_one_page(k, p1);
  }

};

}
