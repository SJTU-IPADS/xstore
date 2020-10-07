#pragma once

#include "../datastream/stream.hpp"
#include "../mega_pager.hpp"

#include "naive_tree.hpp"
#include "page_addr.hpp"

namespace fstore {

namespace store {
/*!
  Iterate all key-value pair in the NaiveBtree.
  After sequences of calling next, the stream will return (k0,v0),(k1,v1), ...,
  (kn,vn), in the sorted order.
*/
template<typename KEY, typename VAL, template<class> class P = BlockPager>
class BNaiveStream : public datastream::StreamIterator<KEY, VAL>
{
  typedef LeafNode<KEY, VAL, P> Leaf;

public:
  BNaiveStream(NaiveTree<KEY, VAL, P>& t, KEY seek_key)
    : start_page(t.safe_find_leaf_page(seek_key))
  {
    begin();
  }

  void begin() override { cur_page = start_page; }

  bool valid() override
  {
    return (cur_page != nullptr) && (cur_idx_in_page < cur_page->num_keys);
  }

  void next() override
  {
    assert(valid());
    cur_idx_in_page += 1;
    if (cur_idx_in_page == cur_page->num_keys) {
      cur_idx_in_page = 0;
      cur_page = (Leaf*)(cur_page->right);
    }
  }

  KEY key() override
  {
    assert(valid());
    return cur_page->keys[cur_idx_in_page];
  }

  VAL value() override
  {
    assert(valid());
    return cur_page->values[cur_idx_in_page];
  }

private:
  Leaf* start_page = nullptr;
  Leaf* cur_page = nullptr;
  uint cur_idx_in_page = 0;
}; // end class

/*!
  Iterate all key-value pair in the NaiveBtree.
  Unlike BNaiveStream, this stream will return the "virtual mapping" of records
  in the pages, namely [page_id, offset in page] pointer pairs.
  So after sequences of calling next, the stream will return (k0,p0),(k1,p1),
  ..., (kn,pn), in the sorted order.
*/
template<typename KEY, typename VAL, template<class> class P = BlockPager>
class BAddrStream : public datastream::StreamIterator<KEY, page_id_t>
{
  typedef LeafNode<KEY, VAL, P> Leaf;

public:
  // leverage the MegaPager's ID encoder as the default encoder
  typedef MegaPager::PPageID PPageID;
  BAddrStream(NaiveTree<KEY, VAL, P>& t, KEY seek_key)
    : start_page(t.safe_find_leaf_page(seek_key))
  {
    this->begin();
  }

  void begin() override { cur_page = start_page; }

  bool valid() override
  {
    return cur_page != nullptr && cur_idx_in_page < cur_page->num_keys;
  }

  void next() override
  {
    assert(valid());
    cur_idx_in_page += 1;
    if (cur_idx_in_page >= cur_page->num_keys) {
      cur_idx_in_page = 0;
      cur_page = (Leaf*)(cur_page->right);
    }
  }

  KEY key() override
  {
    assert(valid());
    return cur_page->keys[cur_idx_in_page];
  }

  page_id_t value() override
  {
    assert(valid());
    return PPageID::encode(P<Leaf>::page_id(cur_page), cur_idx_in_page);
  }

private:
  Leaf* start_page = nullptr;
  Leaf* cur_page = nullptr;
  u32 cur_idx_in_page = 0;
}; // end class

/*!
  Iterate all key-value pair in the NaiveBtree.
  After sequences of calling next, the stream will return (k0,m0),(k1,m1), ...,
  (kn,mn), where mega is the mega address after mapped.
*/
template<typename KEY, typename VAL, template<class> class P = BlockPager>
class BMegaStream : public datastream::StreamIterator<KEY, u64>
{
  typedef LeafNode<KEY, VAL, P> Leaf;

public:
  BMegaStream(NaiveTree<KEY, VAL, P>& t, KEY seek_key, MegaPager* mega)
    : cur_page(t.safe_find_leaf_page(seek_key))
    , pager(mega)
  {}

  void begin() override
  {
    /**
     * Note do nothing!
     * Since during the streaming flowing, it do modifications to *mega pager*,
     * so we do nothing at the begin call.
     */
    calculate_mega();
  }

  bool valid() override
  {
    return cur_page != nullptr && cur_idx_in_page < cur_page->num_keys;
  }

  void next() override
  {
    assert(valid());
    cur_idx_in_page += 1;
    if (cur_idx_in_page >= cur_page->num_keys) {
      cur_idx_in_page = 0;
      cur_page = (Leaf*)(cur_page->right);
    }
    calculate_mega();
  }

  KEY key() override
  {
    assert(valid());
    return cur_page->keys[cur_idx_in_page];
  }

  u64 value() override { return calculated_mega; }

private:
  MegaPager* pager = nullptr;
  Leaf* cur_page = nullptr;
  u64 calculated_mega = 0;
  u32 cur_idx_in_page = 0;

  void calculate_mega()
  {
    if (valid()) {
      // calculate the mega address here to ensure exactly once calling
      calculated_mega =
        pager->emplace(P<Leaf>::page_id(cur_page), cur_idx_in_page);
    }
  }
};

/*!
  Iterate the tree, but returns the sequence of pages as a stream.
 */
template<typename KEY, typename VAL, template<class> class P = BlockPager>
class BPageStream
  : public datastream::StreamIterator<page_id_t, LeafNode<KEY, VAL, P>*>
{
  typedef LeafNode<KEY, VAL, P> Leaf;

public:
  BPageStream(NaiveTree<KEY, VAL, P>& t, KEY seek_key)
    : start_page(t.safe_find_leaf_page(seek_key))
  {
    begin();
  }

  void begin() override { cur_page = start_page; }

  bool valid() override { return cur_page != nullptr; }

  void next() override
  {
    assert(valid());
    cur_page = cur_page->right;
  }

  page_id_t key() override { return P<Leaf>::page_id(cur_page); }

  Leaf* value() override { return cur_page; }

private:
  Leaf* start_page = nullptr;
  Leaf* cur_page = nullptr;
};

} // store

} // end namespace fstore
