#pragma once

#include "common.hpp"
#include "internal/tables.hpp"
#include "r2/src/rpc/rpc.hpp"

namespace fstore
{

namespace server
{

/*!
  The handler's RPC ids
*/
enum
{
  GET_ID = ::r2::rpc::RESERVED_RPC_ID, // 3
  PUT_ID,                              // put (k,v) to the KV-store
  INSERT_ID,
  QUERY_ID, // query the table ID given the table name
  NULL_ID,  // a null RPC handler, just serves the optimal performance of RPC
  META_TAB,
  META_SERVER, // get server meta data, such as memory region size
  CREATE_QP,   // on-demand create client QP
  DELETE_QP,   // delete the client QP
  MODEL_META,  // fetch the model meta data from the server
  PREDICT,     // predict the position of key using RPC, just serves as verify
               // correctness
  SCAN_RPC,    // the scan call of RPC
  //CHECK,

  TREE_META,
};

/**
 * Request data structures
 * Different data structures communicated between server/clients
 */
#pragma pack(1)
struct NullPayload
{
  u64 payload; // the number of bytes should in the reply
  i32 padding;
};

#pragma pack(1)
struct __attribute__ ((packed)) GetPayload
{
  i32 table_id;
  u64 key;
  u32 model_id;
  u32 model_seq;
  u64 model_key;
};

#pragma pack(1)
struct ScanPayload
{
  i32 table_id;
  u64 start;
  u64 num;
  u32 key_per_req;
};

#pragma pack(1)
struct ScanReply
{
  u32 keys_fetched;
};

using PutPayload = GetPayload;

#pragma pack(1)
struct QPRequest
{
  u64 id;
};

#pragma pack(1)
struct QueryPayload
{
  char table_name[max_table_name_len + 1];
};

using TableModel = QPRequest;

/**
 * Reply data structures
 */
/*!
  Meta data info
  - page_addr: the start address of page area
  - page_area_sz: the total size of allocated page
  - num_threads:  total worker thread used at each server
 */
#pragma pack(1)
struct ServerMeta
{
  u64 global_rdma_addr = 0;
  u64 page_addr = 0;
  u64 page_area_sz = 0;
  u64 num_threads = 0;
};

#pragma pack(1)
struct  ModelMeta
{
  u32 dispatcher_sz;
  u64 submodel_buf_addr;
  u32 num_submodel;
  u64 max_addr;
  u64 base_addr;
};

#pragma pack(1)
struct TreeRoot
{
  u64 addr;
  u64 base;
};

/*!
  \note: we assumes the underlying model is two-stage
  This structure returns the meta data defined in the learned index in mousika
  reporsity.
 */
#pragma pack(1)
struct TableModelConfig
{
  u64 first_buf_addr;
  u64 first_buf_sz;
  u64 second_buf_addr;
  u64 second_buf_sz;
  u64 total_keys;
  u64 key_n;
  u64 mega_addr;
  u64 mega_sz;
  u64 tree_depth;
};

} // end namespace server

} // end namespace fstore
