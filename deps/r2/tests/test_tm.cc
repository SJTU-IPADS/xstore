#include <gtest/gtest.h>

#include "../src/timeout_manager.hpp"
#include "../src/scheduler.hpp"

using namespace r2;

namespace test
{

TEST(TM, Order)
{
    TM tm;
    auto cur_time = read_tsc();
    tm.enqueue(0, cur_time + 1000, 0);
    tm.enqueue(1, cur_time + 12, 0);
    tm.enqueue(4, cur_time + 12040, 0);
    tm.enqueue(5, cur_time + 6, 0);

    sleep(1);

    std::vector<u8> ids;
    TMIter it(tm, read_tsc());
    while (it.valid())
    {
        ids.push_back(it.next().first);
    }

    ASSERT_EQ(ids[0], 5);
    ASSERT_EQ(ids[1], 1);
    ASSERT_EQ(ids[2], 0);
    ASSERT_EQ(ids[3], 4);
}

TEST(TM, Scheduler)
{
    RScheduler r;
    r.spawnr([](R2_ASYNC) {
        R2_EXECUTOR.emplace(R2_COR_ID(), 1, [](std::vector<int> &) {
            // wait for a future which never schedules the routine back
            return std::make_pair(NOT_READY, 0);
        });

        // yield, then wait for 10000 cycles and test we do timeout
        auto res = R2_WAIT_FOR(10000);
        ASSERT_EQ(res, TIMEOUT);
    });
    r.run();
}

} // namespace test