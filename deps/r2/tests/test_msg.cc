#include <gtest/gtest.h>

#include "../src/nmsg/msg_factory.hpp"

using namespace r2;

namespace test
{

TEST(MsgDataTest, Basic)
{
    u64 all_heap_sz = 1024 * 1024 * 128;
    AllocatorMaster<1>::init((char *)malloc(all_heap_sz), all_heap_sz);

    AllocMsg msg(AllocatorMaster<1>::get_allocator(), 1024);

    u32 cur_size = 0;
    while (cur_size < 1024)
    {
        u64 *ptr = msg.interpret_as<u64>(cur_size);
        ASSERT(ptr != nullptr);
        *ptr = cur_size;

        cur_size += sizeof(u64);
    }

    CopyMsg msg1(AllocatorMaster<1>::get_allocator(), &msg);

    // real test
    cur_size = 0;
    while (cur_size < 1024)
    {
        u64 *ptr = msg1.interpret_as<u64>(cur_size);
        ASSERT(ptr != nullptr);
        ASSERT_EQ(*ptr, cur_size);
        cur_size += sizeof(u64);
    }

    LOG(4) << "msg test done";
}

} // namespace test