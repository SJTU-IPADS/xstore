#pragma once

#include "datastream/stream.hpp"
#include "constants.hpp"
#include "schema.hpp"

namespace fstore {

namespace sources {

namespace tpcc {

class StockGenerator : public datastream::StreamIterator<stock_key,stock_value> {
 public:
  /*!
    Generate stocks from [start_w_id, end_w_id], inclusively
  */
  StockGenerator(u64 start_w_id,u64 end_w_id,u64 seed = 0xdeadbeaf) :
      start_warehouse_(start_w_id),
      end_warehouse_(end_w_id),
      rand(seed) {
    ASSERT(end_warehouse_ >= start_warehouse_);
  }

  void begin() override {
    current_loaded_ = 0;
  }

  bool valid() override {
    return current_loaded_ < total_stocks();
  }

  void next() override {
    current_loaded_ += 1;
  }

  stock_key key() override {
    u64 ware_id = current_loaded_ / num_items_per_warehosue;
    u64 stock_id = current_loaded_ % num_items_per_warehosue;
    return stock_key(ware_id + start_warehouse_,stock_id);
  }

  stock_value value() override {
    return {
      // This value is very small
            .lock = 0,
      .s_quantity = (u16)(rand.rand_number(stock_min_quantity,stock_max_quantity)),
      .s_ytd = 0,
      .s_order_cnt = 0,
      .s_remote_cnt = 0
    };
  }

  u64 total_warehouses() const {
    return end_warehouse_ - start_warehouse_ + 1;
  }

  u64 total_stocks() const {
    return total_warehouses() * num_items_per_warehosue;
  }

 private:
  const u64 start_warehouse_;
  const u64 end_warehouse_;
  r2::util::FastRandom rand;
  u64 current_loaded_ = 0;
};

}

} // end namespace sources

} // end namespace fstore
