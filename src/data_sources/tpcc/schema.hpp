#pragma once

#include "../../common.hpp"
#include "./constants.hpp"

namespace fstore {

namespace sources {

namespace tpcc {

struct stock_key {
  stock_key(uint32_t w,uint32_t s) : warehouse_id(w),stock_id(s) {
  }

  u64 warehouse_id : 32;
  u64 stock_id     : 32;

  u64 to_u64() const {
    return (static_cast<u64>(warehouse_id) << 32) | stock_id;
  }
};

struct stock_value {
  u64     lock;
  u16     s_quantity;
  float   s_ytd;
  u32     s_order_cnt;
  u32     s_remote_cnt;
};

inline bool sanity_check_stock(const stock_value &value) {
  // TODO: not implemented
  return true;
}

struct district_key
{
  u64 v;
  district_key(u32 wid, u32 d_id)
    : v(d_id * kMaxDistrictPerW)
  {}
};

struct district_value
{
  u64   lock;
  i64   ytd;
  float tax;
  i32   next_o_id;
  char  name[10];
  char  street_1[20];
  char  street_2[20];
  char  city[20];
  char  state[2];
  char  zip[9];
};

struct order_key
{
  u64 v;
  order_key(u32 wid,u32 did,u32 oid)
  {
    auto upper = wid * kMaxDistrictPerW + did;
    v = static_cast<u64>(upper) << 32 | static_cast<u64>(oid);
  }
};

struct order_value
{
  u32 cid;
  u32 carrier_id;
  u8  ol_cnt;
  bool all_local;
  u32 o_entry_d;
};

struct oidx_key
{
  u64 v;
  oidx_key(u32 wid, u32 did, u32 cid, u32 oid) {
    u32 upper_id = (wid * kMaxDistrictPerW + did) * kMaxCustomerPerD + cid;
    v = (static_cast<u64>(upper_id) << 32) | static_cast<u64>(oid);
  }
};

struct no_key
{
  u64 v;
  no_key(u32 wid, u32 did, u32 oid)
  {
    u32 upper_id = (wid * kMaxDistrictPerW + did);
    v = (static_cast<u64>(upper_id) << 32) | static_cast<u64>(oid);
  }
};

// orderline key
struct ol_key
{
  u64 v;
  ol_key(u32 wid, u32 did, u32 oid, u32 number) {
    auto upper = wid * kMaxDistrictPerW + did;
    u64 ooid = upper * 10000000 + static_cast<u64>(oid);
    v = ooid * 15 + number;
  }
};

struct ol_value
{
  u32 i_id; // item
  u32 delivery_d;
  u32 ol_amount;
  u32 ol_supply_w_id;
  u8  ol_quantity;
};

} // namespace tpcc
}

} // end namespace fstore

