#pragma once

#include "common.hpp"

namespace fstore {

enum {
  INVALID_CURSOR
};

/*!
  The scan context serves as a cursor to the database.
  The DB should first acquire a context, and uses the context to fetch other records in batches.
 */
struct ScanContext {
  ScanContext(u64 cursor, u64 key,const std::tuple<u64,u64> &range) :
      cursor(cursor), in_scaned_key(key), key_range(range) {
  }

  bool finish() const {
    return in_scaned_key >= std::get<1>(key_range);
  }

  // cursor, page_id
  i64 cursor = INVALID_CURSOR;

  u64 in_scaned_key;

  /*!
    The scan range. [start,end)
   */
  std::tuple<u64,u64> key_range;
};

} // fstore
