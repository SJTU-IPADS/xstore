#pragma once

#include "table.hpp"
#include <vector>
#include <map>

namespace fstore {

namespace server {

using namespace store;

const u16 max_table_name_len = 15;
const u16 max_table_supported = 16;

enum {
  TABLE_NOT_EXSIST
};

/*!
  Tables are identified by string or i32.
  TableNaming stores all a mapping between string -> i32.
 */
class TableNaming {
 public:
  i32 register_table(const std::string &name) {
    if(mapping.find(name) != mapping.end())
      return mapping[name];
    mapping.insert(std::make_pair(name,alloc_id ++));
    return alloc_id - 1;
  }

  i32 get_table_id(const std::string &name) {
    if(mapping.find(name) == mapping.end())
      return TABLE_NOT_EXSIST;
    return mapping[name];
  }

 private:
  std::map<std::string,u64> mapping;
  i32                       alloc_id = 0;
};

/*!
  Tables manage all registered tables.
 */
class Tables {
 public:
  Tables() = default;

  i32 get_table_id(const std::string &name) {
    std::lock_guard<std::mutex> guard(lock);
    auto id = naming.get_table_id(name);
    return id;
  }

  i32 register_table(const std::string &name,const std::string &model) {
    std::lock_guard<std::mutex> guard(lock);
    auto id = naming.register_table(name);
    if(id == tables.size()) {
      tables.emplace_back(name,model);
    }
    return id;
  }

  Table &get_table(i32 id) {
    ASSERT(id < tables.size()) << "get wrong table id: " << id;
    return tables[id];
  }

 private:
  TableNaming               naming;
  std::vector<Table>        tables;

  std::mutex                lock;
}; // end class Tables

} //

} // fstore
