#include <gtest/gtest.h>

#include "../src/utils/memory_util.hpp"
#include "r2/src/allocator_master.hpp"

using namespace fstore;
using namespace fstore::utils;
using namespace r2;

namespace test {

TEST(Allocator,Basic) {
  u64 mem_size = 128 * MB;
  char *memory = new char[mem_size];

  AllocatorMaster<0>::init(memory,mem_size);

  for(uint i = 0;i < 4;++i) {
    auto all = AllocatorMaster<0>::get_allocator();
    ASSERT_NE(all,nullptr);

    void *previous_ptr = all->alloc(80);
    for(uint i = 0;i < 1024;++i) {
      auto ptr = all->alloc(80);
      ASSERT_TRUE(AllocatorMaster<0>::within_range(ptr));
      ASSERT_LE((u64)ptr - (u64)previous_ptr,80);
      previous_ptr = ptr;
    }
  }
  delete[] memory;
}

}
