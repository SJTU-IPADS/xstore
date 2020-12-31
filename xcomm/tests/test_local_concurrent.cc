#include <gtest/gtest.h>
#include <memory>

#include <atomic>

#include "../../deps/r2/src/thread.hh"
using namespace r2;

#include "../src/atomic_rw/local_rw_op.hh"
#include "../src/atomic_rw/mod.hh"

using namespace xstore::xcomm::rw;

#include "./utils.hh"

namespace test {
TEST(AtomicRW, local_concurrent_rw_64) {
  using TestThread = r2::Thread<usize>;

  using TO = TestObj<256>;
  using Obj64 = WrappedType<TO>;
  LOG(4) << "test obj sz:" << sizeof(Obj64) << "; internal obj sz: " << sizeof(TO);

  Obj64 o;
  inplace_rand_str(o.get_payload().data, o.get_payload().sz());
  o.get_payload().checksum =
      ::test::simple_checksum(o.get_payload().data, o.get_payload().sz());
  LOG(4) << "initial checksum: " << o.get_payload().checksum;
  //verbose_simple_checksum(o.get_payload().data, o.get_payload().sz());
  r2::compile_fence();

  std::vector<u64> past_seqs(10000001, 0);
  usize succ_read_cnt = 0;

  std::atomic<bool> update_exit(false);
  // spawn an update thread to update 'o'
  std::unique_ptr<TestThread> modify_thread =
      std::make_unique<TestThread>([&o, &succ_read_cnt, &past_seqs, &update_exit]() -> usize {
        for (uint i = 0; i < 100000000; ++i) {
          auto str = ::test::random_string(o.get_payload().sz());
          ASSERT(o.get_payload().sz() == str.size());
          auto checksum = ::test::simple_checksum(str.data(), str.size());

          if (o.seq >= past_seqs.size()) {
            break;
          }
          ASSERT(checksum != 0);
          past_seqs[o.seq + 1] = checksum;

          r2::compile_fence();
          // update
          auto res = o.begin_write();
          {
            LocalRWOp().write(MemBlock(o.get_payload().data, str.size()),
                              MemBlock((char *)str.data(), str.size()));

            o.get_payload().checksum = checksum;
            ASSERT(!o.consistent());
          }
          o.done_write(res);
          auto re_checksum = ::test::simple_checksum(o.get_payload().data,
                                                     o.get_payload().sz());
          ASSERT(re_checksum == checksum) << re_checksum << " " << checksum;

          // FIXME: shall we add some sleep?
        }
        LOG(4) << "update thread exit, succ read cnt: " << succ_read_cnt;
        update_exit = true;
        return 0;
      });

  // spawn a read thread to concurrently read 'o'
  // it will using checksum to check that the read is consistent using the
  // AtomicRW interfaces
  std::unique_ptr<TestThread> read_thread =
      std::make_unique<TestThread>([&o, &succ_read_cnt, &past_seqs, &update_exit]() -> usize {
        LocalRWOp op;
        OrderedRWOp op1;

        MemBlock src(&o, sizeof(Obj64));
        MemBlock dst(new char[sizeof(Obj64)], sizeof(Obj64));

        while (!update_exit) {
          //op.read(src, dst);  // shoud fail
          AtomicRW().atomic_read<TO>(op, src, dst); // should work
          Obj64 *ro = reinterpret_cast<Obj64 *>(dst.mem_ptr);
          const usize ccc = ::test::simple_checksum(ro->get_payload().data,
                                                    ro->get_payload().sz());
          const usize compare = static_cast<usize>(ro->get_payload().checksum);
          ASSERT(ccc == compare)
              << "ccc: " << ccc << " " << compare << "; " << 0x0 << " "
              << ro->seq << "; seqs: " << ro->seq_check
              << "; past checks: " << past_seqs[ro->seq] << " " <<  past_seqs[ro->seq + 1]
              << " " << past_seqs[ro->seq - 1];
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
