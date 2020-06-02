#include <gtest/gtest.h>
#include <memory>

#include "../../deps/r2/src/thread.hh"
using namespace r2;

#include "../src/atomic_rw/local_rw_op.hh"
#include "../src/atomic_rw/mod.hh"

template <usize N> struct TestObj {
  char data[N];
  u64 checksum = 0;
  inline auto sz() -> usize { return N; }
};

using namespace xstore::xcomm::rw;

#include "./utils.hh"

namespace test {
TEST(AtomicRW, local_concurrent_rw_64) {
  using TestThread = r2::Thread<usize>;

  using TO = TestObj<64 - sizeof(u32) - sizeof(u32) - sizeof(u64)>;
  using Obj64 = WrappedType<TO>;
  //ASSERT_EQ(sizeof(Obj64), 64);
  Obj64 o;
  inplace_rand_str(o.get_payload().data, o.get_payload().sz());
  o.get_payload().checksum =
      ::test::simple_checksum(o.get_payload().data, o.get_payload().sz());
  LOG(4) << "initial checksum: " << o.get_payload().checksum;
  //verbose_simple_checksum(o.get_payload().data, o.get_payload().sz());
  r2::compile_fence();

  std::vector<u64> past_seqs(10000001, 0);
  usize succ_read_cnt = 0;

  // spawn an update thread to update 'o'
  std::unique_ptr<TestThread> modify_thread =
      std::make_unique<TestThread>([&o, &succ_read_cnt, &past_seqs]() -> usize {
        for (uint i = 0; i < 10000000; ++i) {
          auto str = ::test::random_string(o.get_payload().sz());
          ASSERT(o.get_payload().sz() == str.size());
          auto checksum = ::test::simple_checksum(str.data(), str.size());

          ASSERT(o.seq < past_seqs.size()) << o.seq;
          ASSERT(checksum != 0);
          past_seqs[o.seq] = checksum;

          r2::compile_fence();
          // update
          o.begin_write();
          {
            LocalRWOp().write(MemBlock(o.get_payload().data, str.size()),
                              MemBlock((char *)str.data(), str.size()));

            ASSERT(memcmp(o.get_payload().data, str.data(), str.size()) == 0);
            auto re_checksum = ::test::simple_checksum(o.get_payload().data,
                                                       o.get_payload().sz());

            ASSERT(re_checksum == checksum)
                << re_checksum << " " << checksum << "; at :" << i;
            ASSERT(checksum != 0);
            r2::compile_fence();
            o.get_payload().checksum = checksum;
            r2::compile_fence();
          }
          o.done_write();
          auto re_checksum = ::test::simple_checksum(o.get_payload().data,
                                                     o.get_payload().sz());
          ASSERT(re_checksum == checksum) << re_checksum << " " << checksum;

          // FIXME: shall we add some sleep?
        }
        LOG(4) << "update thread exit, succ read cnt: " << succ_read_cnt;
        return 0;
      });

  // spawn a read thread to concurrently read 'o'
  // it will using checksum to check that the read is consistent using the
  // AtomicRW interfaces
  std::unique_ptr<TestThread> read_thread =
      std::make_unique<TestThread>([&o, &succ_read_cnt, &past_seqs]() -> usize {
        LocalRWOp op;
        MemBlock src(&o, sizeof(Obj64));
        MemBlock dst(new char[sizeof(Obj64)], sizeof(Obj64));

        for (uint i = 0; i < 1000000; ++i) {
          //op.read(src, dst);  // shoud fail
          AtomicRW::atomic_read<TO>(op, src, dst); // should work
          Obj64 *ro = reinterpret_cast<Obj64 *>(dst.mem_ptr);
          const usize ccc = ::test::simple_checksum(ro->get_payload().data,
                                                    ro->get_payload().sz());
          const usize compare = static_cast<usize>(ro->get_payload().checksum);
          ASSERT(ccc == compare)
              << "ccc: " << ccc << " " << compare << "; " << 0x0 << " "
              << ro->seq << "; seqs: " << ro->seq_check
              << "; past checks: " << past_seqs[ro->seq];
          ASSERT(ro->consistent());
          succ_read_cnt += 1;
        }
        LOG(4) << "succ read cnt: " << succ_read_cnt;
        ASSERT(succ_read_cnt > 0);
        return 0;
      });

  read_thread->start();
#if 1
  modify_thread->start();
  modify_thread->join();
#endif
  read_thread->join();
}
} // namespace test

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
