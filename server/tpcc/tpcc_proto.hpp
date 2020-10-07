#pragma once

#include "common.hpp"

#include "../internal/tables.hpp"

#include "r2/src/rpc/rpc.hpp"

namespace fstore {

namespace server {

enum
{
  NO_ID = ::r2::rpc::RESERVED_RPC_ID, // execute no
  FETCH_ORDER,
  FETCH_OLS, // fetch order lines
};

struct NOArg
{
  u32 warehouse_id;
  u32 district_id;
  u32 cust_id;
  u64 stocks[15];
  u8  num_stocks;
};

struct FOArg
{
  u64 seek_key;
};

} // namespace server

} // namespace fstore
