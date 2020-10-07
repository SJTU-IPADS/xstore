#pragma once

#include "handlers.hpp"
#include "control_handlers.hpp"
#include "lidx_handler.hpp"

namespace fstore {

namespace server {

class Controler {
 public:
  static void register_all(RPC &rpc) {
    DataHandlers::register_all(rpc);
    ControlHandlers::register_all(rpc);
    LRHandlers::register_all(rpc);
  }
};

} // server

} // fstore
