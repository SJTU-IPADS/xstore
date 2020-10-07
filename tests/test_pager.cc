#include <gtest/gtest.h>

#include "../src/pager.hpp"

using namespace fstore;

namespace test {

struct __attribute__ ((aligned (64))) FPage {
  char data[3333];
};

TEST(Pager, Block) {

  char *buf = new char[1024 * 1024 * 1024];

  typedef BlockPager<FPage> Pager;
  Pager::init(buf + 12,1024 * 1024 * 1024);
  ASSERT_NE(Pager::blocks,nullptr);
  //ASSERT_EQ(reinterpret_cast<u64>(Pager::blocks) % sizeof(FPage),0);

  for(uint i = 0; i < 1024;++i) {
    auto ptr = Pager::allocate_one();
    ASSERT_NE(ptr,nullptr);

    auto id = Pager::page_id(ptr);
    ASSERT_EQ(id,i);
  }

  delete[] buf;
}

}
