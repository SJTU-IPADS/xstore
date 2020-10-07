#pragma once

#include "datastream/stream.hpp"

#include "mega_pager_v2.hpp"
#include "page_addr.hpp"

namespace fstore {

#define FAKE_KEYS 0

/*!
  Iterate through the trees, generate key and their corresponding mega_id.
  Important:
   The iterator will convert the original key in the space by *2, and create
  fake keys if necessary. The fake keys are important so that the space of
  MegaID is continues. More specifically:
   - old keys are not valid. (which means not in the original B+tree).
   - iter() return boths original keys (by *2) and fake keys (by return the last
  entry in one page and plus one). This iterator will return more keys than that
  in B+tree. But it is ok, the more keys are only used for training, not for
  storage.

   Currently this mega iterator only supports u64 key.
 */

/*!
  The MegaFaker handles the key fake process.
 */
class MegaFaker
{
public:
  static u64 encode(u64 k)
  {
#if FAKE_KEYS
    return k << 1;
#else
    return k;
#endif
  }

  static u64 decode(u64 k)
  {
#if FAKE_KEYS
    return k >> 1;
#else
    return k;
#endif
  }

  /*!
    Generate the fake key from a *original* key.
   */
  static u64 fake(u64 k)
  {
#if FAKE_KEYS
    return encode(k) + 1;
#else
    return k;
#endif
  }

  static bool is_fake(u64 k)
  {
#if FAKE_KEYS
    return k % 2 == 1;
#else
    return false;
#endif
  }

  /*!
    verify whether the key is fakable, say,
    decode(encode(k)) == k;
   */
  static bool verify_fakable(u64 k)
  {
#if FAKE_KEYS
    auto res = encode(k);
    r2::compile_fence();
    return decode(res) == k;
#else
    return true;
#endif
  }
};

/*!
  Encode the seq into the page id
  page_id (64 bits) | reserved bits | 8-bit size | - 8-bit seq - | 32-bit
  physical_id |
*/
class SeqEncode
{
public:
  static const constexpr page_entry_t physical_bits = 32;
  static const constexpr page_entry_t seq_bits = 8;

  static page_entry_t encode(const page_entry_t& sz,
                             const page_entry_t& seq,
                             const page_entry_t& id)
  {
    return (sz << (seq_bits + physical_bits)) |
           (static_cast<page_entry_t>(seq) << physical_bits) | id;
  }

  static page_entry_t decode_seq(const page_entry_t& encoded)
  {
    constexpr auto mask =
      utils::bitmask<page_entry_t>(physical_bits + seq_bits);
    return (encoded & mask) >> physical_bits;
  }

  static page_entry_t decode_sz(const page_entry_t& encoded)
  {
    return (encoded) >> (physical_bits + seq_bits);
  }

  static page_entry_t decode_id(const page_entry_t& encoded)
  {
    return encoded & (utils::bitmask<page_entry_t>(physical_bits));
  }
};

/*!
  So we then make a wrapper over all the pages.
 */
template<typename PAGE_TYPE>
class MegaStream : public datastream::StreamIterator<u64, mega_id_t>
{
public:
  MegaStream(datastream::StreamIterator<page_id_t, PAGE_TYPE*>* iter,
             MegaPagerV<PAGE_TYPE>* mp)
    : iter(iter)
    , pager(mp)
  {
    begin();
  }

  void begin() override
  {
    iter->begin();
    if (iter->valid()) {
      cur_page = iter->value();
    }
  }

  bool valid() override
  {
    return cur_page != nullptr && cur_idx_in_page < PAGE_TYPE::page_entry();
  }

  void next() override
  {
    assert(valid());
    cur_idx_in_page += 1;
    auto threshold = PAGE_TYPE::page_entry();
#if !FAKE_KEYS
    threshold = cur_page->num_keys;
#endif
    // LOG(4) << "next: " << cur_idx_in_page << " total: " << threshold;
    if (cur_idx_in_page == threshold) {
      iter->next();
      if (iter->valid()) {
        cur_page = iter->value();
        cur_idx_in_page = 0;
      } else {
        cur_page = nullptr;
        cur_idx_in_page = PAGE_TYPE::page_entry();
      }
    }
  }

  /*!
    \note: return the faked key
   */
  u64 key() override
  {
    // to support dynamic workloads, we encode the seq into page_id
    ASSERT(MegaFaker::verify_fakable(cur_page->keys[cur_idx_in_page]));
    if (cur_idx_in_page < cur_page->num_keys)
      return MegaFaker::encode(cur_page->keys[cur_idx_in_page]);
    // ASSERT(false) << "get cur_idx_in_page: " << cur_idx_in_page << "; total:
    // " << cur_page->num_keys;
    assert(cur_page->num_keys > 0);
    return MegaFaker::fake(cur_page->keys[cur_page->num_keys - 1]);
  }

  mega_id_t value() override
  {
    auto threshold = PAGE_TYPE::page_entry();
#if !FAKE_KEYS
    threshold = cur_page->num_keys;
#endif
    ASSERT(SeqEncode::decode_seq(
             SeqEncode::encode(threshold, cur_page->seq, iter->key())) ==
           (page_entry_t)(cur_page->seq))
      << "seq: " << (page_entry_t)(cur_page->seq) << "; decoded: "
      << SeqEncode::decode_seq(
           SeqEncode::encode(0, cur_page->seq, iter->key()));
    return pager->emplace(
      SeqEncode::encode(threshold, cur_page->seq, iter->key()),
      cur_idx_in_page,
      threshold);
  }

private:
  MegaPagerV<PAGE_TYPE>* pager = nullptr;
  PAGE_TYPE* cur_page = nullptr;
  page_entry_t cur_idx_in_page = 0;
  datastream::StreamIterator<page_id_t, PAGE_TYPE*>* iter;
};

} // end namespace fstore
