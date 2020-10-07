#pragma once

#include "../workloads.hpp"

#include "constants.hpp"
#include "schema.hpp"

namespace fstore {

namespace sources {

namespace tpcc {

class StockWorkload : public BaseWorkload<StockWorkload> {
 public:
  StockWorkload(u64 start_w,u64 end_w,u64 seed = 0xdeadbeaf) :
      start_ware(start_w),
      end_ware(end_w),
      rand(seed) {
    ASSERT(start_ware <= end_ware);
  }

  u64 next_key_impl() {
    auto item_id = rand.rand_number(0,num_items_per_warehosue - 1);
    auto ware_id = rand.rand_number(start_ware,end_ware);
    return stock_key(ware_id,item_id).to_u64();
  }

 public:
  const u64 start_ware;
  const u64 end_ware;

 private:
  r2::util::FastRandom rand;
};

}


}

} // end namespace fstore
