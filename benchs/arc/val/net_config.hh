#pragma once

#include "common.hpp" // fstore's common

namespace fstore {

namespace platforms {

/*!
  \Note: This file is only used for temporal usage,
  which choose the corresponding RDMA nic device on our VAL cluster.
  This is because each VAL machine has multiple nics,
  choosing the appropriate NIC is essential to the performance.

  Here is our detailed NIC configuration:
  - 24 cores, 2 CX4 NIC.

  The user must provide its corresponding NIC choosing logic for better
  performance.
 */
class VALNic
{
public:
  static const int core_per_socket = 12;

  static u32 choose_nic(u32 tid)
  {
    if (tid >= core_per_socket) {
      /*
       * using the first NIC for the thread with id 12-23,
       * this choice is based on our evaluations.
       */
      return 1;
    }
    return 0;
  }
};

} // end namespace platform

}
