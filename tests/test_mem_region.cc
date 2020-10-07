#include <gtest/gtest.h>

#include "../src/mem_region.hpp"

namespace test {

using namespace fstore;

TEST(Region, Mem) {

  u64 mem_size = 2 * MB;
  RegionManager rm(mem_size);

  ASSERT_EQ(rm.register_region("test1", 1 * MB),0);
  ASSERT_EQ(rm.register_region("test2", 1 * MB),1);

  ASSERT_EQ(rm.get_region("test1").get_as_virtual(),rm.get_region(0).get_as_virtual());
  ASSERT_EQ(rm.get_region("test2").get_as_virtual(),rm.get_region(1).get_as_virtual());

  ASSERT_EQ(rm.get_region(0).size + rm.get_region(1).size,mem_size);
}

}
