#pragma once

#include "stream.hpp"
#include "rocks_util.hpp"
#include "rocksdb/db.h"

namespace fstore {

namespace datastream {

/*!
  It's just a wrapper over rocksdb's internal iterator, but fits our API.
*/
template <typename KeyType, typename ValType>
class RocksIter : public StreamIterator<KeyType,ValType> {
 public:
  RocksIter(rocksdb::DB* db) : it(db->NewIterator(rocksdb::ReadOptions())) {
  }

  void begin() override {
    it->SeekToFirst();
  }

  bool valid() override {
    return it->Valid();
  }

  void next() override {
    it->Next();
  }

  KeyType key() override {
    return RocksUtil::from_slice<KeyType>(it->key());
  }

  ValType value() override {
    return RocksUtil::from_slice<ValType>(it->value());
  }

  ~RocksIter() {
    if(it) delete it;
  }

 private:
  rocksdb::Iterator* it = nullptr;
};

} // end namespace datastream

} // end namespace fstore
