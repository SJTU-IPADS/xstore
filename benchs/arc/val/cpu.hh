#pragma once

#include <sched.h>

#include "../../deps/r2/src/common.hh"

namespace xstore {

namespace platforms {

using namespace r2;

/*!
  Hardware setting on VAL:
   // TODO
 */

class VALBinder
{
public:
  static void bind(u64 socket, u64 core)
  {
    u64 id = 0;
    switch (socket) {
      case 0:
        id = socket0_id(core);
        break;
      case 1:
        id = socket1_id(core);
        break;
      default:
        ASSERT(false);
    }

    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(id, &mask);
    sched_setaffinity(0, sizeof(mask), &mask);
  }

  static const u64 core_per_socket() { return 24; }

  static const u64 socket0_id(uint core)
  {
    static const std::array<u64, 24> socket_0 = { 0,  2,  4,  6,  8,  10,
                                                  12, 14, 16, 18, 20, 22,
                                                  24, 26, 28, 30, 32, 34,
                                                  36, 38, 40, 42, 44, 46 };
    return socket_0.at(core);
  }

  static const u64 socket1_id(uint core)
  {
    static const std::array<u64, 24> socket_1 = { 1,  3,  5,  7,  9,  11,
                                                  13, 15, 17, 19, 21, 23,
                                                  25, 27, 29, 31, 33, 35,
                                                  37, 39, 41, 43, 45, 47 };
    return socket_1.at(core);
  }
};

} // platforms

}
