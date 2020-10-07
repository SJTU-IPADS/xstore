#pragma once

#include "../internal/db_traits.hpp"

#include "data_sources/ycsb/stream.hpp"

namespace fstore {
#if 1
using namespace sources::ycsb;

namespace server {

extern TestLidx test_lidx;

class YCSBLoader
{
public:
  static int populate(Tree& t, int num, u64 seed)
  {

    YCSBGenerator it(0, num, seed);
    uint count(0);

    std::vector<u64> keys;

    for (it.begin(); it.valid(); it.next()) {
      ValType val;
      val.set_meta(it.key());
      //keys.push_back(it.key());
      t.put(it.key(), val);
      count += 1;
    }

#if 0
    LOG(4) << "keys sz:" << keys.size();
    ASSERT(keys.size() == count) << "keys sz: "<< keys.size();
    std::sort(keys.begin(),  keys.end());
    for(auto k : keys) {
      ValType val;
      val.set_meta(it.key());
      t.put(it.key(), val);
    }
#endif
    return count;
  }

  static int populate_hash(Tree& t, int num, u64 seed)
  {
    std::set<u64> key_set;
    YCSBHashGenereator it(0, num, seed);
    uint count(0);

    std::vector<u64> all;

    for (it.begin(); it.valid(); it.next()) {
      ValType val;
      val.set_meta(it.key());
      //auto v0 = t.put(it.key(), val);
      all.push_back(it.key());
      //key_set.emplace(it.key());
      //auto v = t.get(it.key());
      //ASSERT(v->get_meta() == it.key());
#if 0
      test_lidx.insert(it.key(),val);
#endif
      count += 1;
    }

    std::sort(all.begin(),all.end());
    for (auto k : all) {
      ValType val;
      auto v0 = t.put(k, val);
    }
    //LOG(4) << "hash real inserted:" << key_set.size();

    return count;
  }

  static void sanity_check_hash(Tree& t, int num)
  {
    YCSBHashGenereator it(0, num);
    for (it.begin(); it.valid(); it.next()) {
      auto v = t.get(it.key());
      ASSERT(v->get_meta() == it.key())
        << "get meta in loader:" << v->get_meta() << " for key: " << it.key();
    }
  }
};

} // server
#endif
} // fstore
