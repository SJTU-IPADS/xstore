#pragma once

#include "../internal/db_traits.hpp"

#include "data_sources/nutanix/stream_1.hpp"

namespace fstore {

using namespace sources::ycsb;

namespace server {

extern TestLidx test_lidx;

class NutLoader
{
public:
  static int populate(Tree& t, int num, u64 seed)
  {

    ::fstore::sources::NutOne<Tree> it(&t, num, seed);
    u64 count(0);

    for (it.begin(); it.valid(); it.next()) {
      ValType val;
      auto key = it.key();
      t.put(key, val);
      count++;
    }
    return count;
  }

  static int populate_simple(Tree& t, u64 num, u64 seed)
  {

    u64 count(0);
    while(count < num) {
      ValType val;
      t.put(count++,val);
    }

    return count;
  }
};

}

}
