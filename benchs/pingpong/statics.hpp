#pragma once

#include "common.hpp"

namespace pingpong {

using namespace fstore;

/*!
 * Statictics used for multi-thread reporting.
 * The structure is 128-byte padded and aligned to avoid false sharing.
 */
struct alignas(128) Statics {
  u64  counter = 0;

  void increment(u16 count = 1) {
    counter += count;
  }

 private:
  char pad[128 - sizeof(u64)];
};

}