#pragma once

namespace fstore {
   const int max_page_fetcher = 2;
}

#include "../server/internal/table.hpp"
#include "../server/proto.hpp"

#include "r2/src/rpc/rpc.hpp"
#include "rlib/rdma_ctrl.hpp"

#include "model_fetcher.hpp"
#include "fclient.hpp"
#include "fclient_scan.hpp"
#include "rpc_scan.hpp"

#include "networked_btree.hpp"
