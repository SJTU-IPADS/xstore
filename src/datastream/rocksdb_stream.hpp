#pragma once

#include <string>

#include "stream.hpp"
#include "rocksdb_iter.hpp"

#include "rocksdb/db.h"

#include "r2/src/common.hpp"

namespace fstore {

namespace datastream {

/*! Datastream loader backed by rocksdb */
template <typename KeyType, typename ValType>
class RocksStream : public Stream<KeyType,ValType> {
 public:
  /*!
    \param db_loc: the directory which stores the DB
  */
  RocksStream(const std::string &db_loc) {
    rocksdb::Options options;
    options.create_if_missing = true;
    auto status = rocksdb::DB::Open(options, db_loc, &db);
    ASSERT(status.ok()) << "failed to open DB";
  }

  bool put(const KeyType &key,const ValType &val) override {
    auto s = db->Put(rocksdb::WriteOptions(), RocksUtil::to_slice(key), RocksUtil::to_slice(val));
    return s.ok();
  }

  void close() override {
    if(db) {
      delete db;
      db = nullptr;
    }
  }

  std::unique_ptr<StreamIterator<KeyType,ValType>> get_iter() override {
    return std::unique_ptr<RocksIter<KeyType,ValType>>(new RocksIter<KeyType,ValType>(db));
  }

  ~RocksStream() {
    close();
  }
 private:
  rocksdb::DB* db = nullptr;
};

}

} // end namespace fstore
