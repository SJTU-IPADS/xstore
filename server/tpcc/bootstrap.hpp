#pragma once

#include "../../src/data_sources/tpcc/schema.hpp"
#include "../../src/stores/naive_tree.hpp"

#include "../../deps//r2/src/random.hpp"

#include "../internal/db_traits.hpp"

#include <unordered_map>

namespace fstore {

using namespace ::fstore::sources::tpcc;
using namespace ::fstore::store;

namespace server {

// no index update table
std::unordered_map<u64, district_value>
  district_table;
std::unordered_map<u64, stock_value>    stock_table;
// index update table, so we need to handle concurrent updates
using TO = NaiveTree<u64, order_value*, MallocPager>;
TO  order_table;
TO  no_table;
using TOL = NaiveTree<u64, ol_value*, MallocPager>;
TOL orderline_table;

DEFINE_uint32(num_ware, 1, "Number of warehouses used");

class TPCCBoot
{
public:
  static void populate(Tree& order_idx) {

    LOG(4) << "start populating TPCC DB";

    // some pre check
    order_table.safe_get_with_insert(0);

    LOG(4) << "pre check pass";

    r2::util::FastRandom rand(0xdeadbeaf);

    // we first populate district table
    for (u32 w = 1; w < 1 + FLAGS_num_ware; w += 1) {
      for (u32 d = 1; d <= kMaxDistrictPerW; d += 1) {
        district_key key (w,d);
        district_value v = {
                          .lock = 1,
                          .ytd = 30000 * 100,
                          .tax = 0, // not important
                          .next_o_id = 3001,
                          // below fields are not important

        };
        district_table.insert(std::make_pair(key.v,v));
      }
    }
    LOG(4) << "populating district done";

    // then we load the stocks
    for (u32 w = 1; w < 1 + FLAGS_num_ware; w += 1) {
      for (u32 i = 0; i <= kNumItems; ++i) {
        stock_key sk(w,i);
        stock_value v = {
         .lock = 1,
         .s_quantity = rand.rand_number<u16>(10, 100),
         .s_ytd = 0,
         .s_order_cnt = 0,
         .s_remote_cnt = 0,
        };
        stock_table.insert(std::make_pair(sk.to_u64(),v));
      }
    }
    LOG(4) << "populate stock done";

#if 1
    // populate orders
    for (u32 w = 1; w < 1 + FLAGS_num_ware; w += 1) {
      for (u32 d = 0; d <= kMaxDistrictPerW; d += 1) {
        for (uint i = 0; i < kMaxCustomerPerD; ++i) {
          for (uint j = 0; j < 5; ++j) {
            ValType val;
            oidx_key k(w, d, i, j);
            order_idx.put(k.v, val);
          }
        }
      }
    }
#endif

    // insert a dummy value
    ValType val;
    order_idx.put(0,val);

    return;
  }
};

} // namespace server

} // namespace fstore
