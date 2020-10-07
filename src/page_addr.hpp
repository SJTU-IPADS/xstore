#pragma once

#include "common.hpp"
#include "pager.hpp"

#include "utils/all.hpp"

namespace fstore {

/*!
  A wrapper class for computing page id's.
  The addr is a [ Page id --- xx bit --- | Offset in page --- xx bit --- ] pair.
  This is stored as a 32-bit unsigned integer.
*/
template <int TOTAL_BITS, int ID_BITS>
class PageID {
 public:
  static constexpr u32 OFFSET_BITS  = TOTAL_BITS - ID_BITS; // we donot support many keys in one page

  static constexpr u64 offset_mask = ::fstore::utils::bitmask<u64>(OFFSET_BITS);

  inline static page_id_t encode(u64 page_id, u64 offset) {
    return page_id << OFFSET_BITS | offset;
  }

  inline static u64 decode_id(page_id_t id) {
    return id >> OFFSET_BITS;
  }

  inline static u64 decode_off(page_id_t id) {
    return id & offset_mask;
  }
};

} // end namespace fstore
