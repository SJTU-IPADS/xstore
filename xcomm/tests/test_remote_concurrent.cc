#include <gtest/gtest.h>

#include <atomic>

#include "../src/atomic_rw/rdma_rw_op.hh"
#include "../src/atomic_rw/local_rw_op.hh"
#include "../src/atomic_rw/wrapper_type.hh"
#include "../src/atomic_rw/mod.hh"

#include "../../deps/rlib/core/lib.hh"

#include "./utils.hh"

#include "../../deps/r2/src/thread.hh"
using namespace r2;

namespace test {

using namespace xstore::xcomm::rw;

TEST(AtomicRW, RemoteRWConcurrent) {
  auto res = RNicInfo::query_dev_names();
  ASSERT_FALSE(res.empty());

  auto nic = std::make_shared<RNic>(res[0]);
  ASSERT_TRUE(nic->valid());

  auto config = QPConfig();
  auto qpp = RC::create(nic, config).value();
  ASSERT_TRUE(qpp->valid());

  // try send an RDMA request
  // init the memory
  auto mem = Arc<RMem>(new RMem(4096)); // allocate a memory with 1K bytes
  ASSERT_TRUE(mem->valid());

  // register it to the Nic
  RegHandler handler(mem, nic);
  ASSERT_TRUE(handler.valid());

  auto mr = handler.get_reg_attr().value();

  RC &qp = *qpp;
  qp.bind_remote_mr(mr);
  qp.bind_local_mr(mr);

  // finally connect to myself
  auto res_c = qp.connect(qp.my_attr());
  RDMA_ASSERT(res_c == IOCode::Ok);

  // now the real test
  char *src_loc = reinterpret_cast<char *>(mem->raw_ptr);
  char *dst_loc = src_loc + 2048;

  using TestThread = r2::Thread<usize>;

  // main test body
  using TO = TestObj<100>;
  using Obj64 = WrappedType<TO>;
  LOG(4) << "test obj sz:" << sizeof(Obj64)
         << "; internal obj sz: " << sizeof(TO);

  Obj64 *o = reinterpret_cast<Obj64 *>(src_loc);
  o->init();
  inplace_rand_str(o->get_payload().data, o->get_payload().sz());
  o->get_payload().checksum =
      ::test::simple_checksum(o->get_payload().data, o->get_payload().sz());
  LOG(4) << "initial checksum: " << o->get_payload().checksum;
  // verbose_simple_checksum(o.get_payload().data, o.get_payload().sz());
  r2::store_fence();

  // the total read counts
  usize succ_read_cnt = 0;

  std::atomic<bool> update_exit(false);
  // spawn an update thread to update 'o'
  std::unique_ptr<TestThread> modify_thread =
      std::make_unique<TestThread>([o, &succ_read_cnt, &update_exit]() -> usize {
        for (uint i = 0; i < 10000000; ++i) {
          auto str = ::test::random_string(o->get_payload().sz());
          ASSERT(o->get_payload().sz() == str.size());
          auto checksum = ::test::simple_checksum(str.data(), str.size());

          ASSERT(checksum != 0);

          r2::compile_fence();
          // update
          auto res = o->begin_write();
          {
            LocalRWOp().write(MemBlock(o->get_payload_ptr()->data, str.size()),
                              MemBlock((char *)str.data(), str.size()));

            o->get_payload().checksum = checksum;
            ASSERT(!o->consistent());
          }
          r2::compile_fence();
          o->done_write(res);
          auto re_checksum = ::test::simple_checksum(o->get_payload().data,
                                                     o->get_payload().sz());
          ASSERT(re_checksum == checksum) << re_checksum << " " << checksum;

          // FIXME: shall we add some sleep?
        }
        LOG(4) << "update thread exit, succ read cnt: " << succ_read_cnt;
        update_exit = true;
        return 0;
      });

    std::unique_ptr<TestThread> read_thread =
        std::make_unique<TestThread>([&o, &succ_read_cnt, &update_exit, qpp, dst_loc]() -> usize {
        LocalRWOp op;
        OrderedRWOp op1;

        MemBlock src(reinterpret_cast<void *>(0), sizeof(Obj64));
        MemBlock dst(dst_loc, sizeof(Obj64));

        while (!update_exit) {
          RDMARWOp op2(qpp);

          //op.read(src, dst);  // shoud fail
          AtomicRW().atomic_read<TO>(op2, src, dst); // should work
          //AtomicRW().atomic_read<TO>(op, src, dst); // should work
          // op2.read(src,dst);
          Obj64 *ro = reinterpret_cast<Obj64 *>(dst.mem_ptr);
          const usize ccc = ::test::simple_checksum(ro->get_payload().data,
                                                    ro->get_payload().sz());
          const usize compare = static_cast<usize>(ro->get_payload().checksum);
          ASSERT(ccc == compare)
              << "ccc: " << ccc << " " << compare << "; " << 0x0 << " "
              << ro->seq << "; seqs: " << ro->seq_check;
          ASSERT(ro->consistent());
          succ_read_cnt += 1;
        }
        LOG(4) << "succ read cnt: " << succ_read_cnt;
        ASSERT(succ_read_cnt > 0);
        return 0;
      });
    read_thread->start();
  modify_thread->start();
  modify_thread->join();
  read_thread->join();
}

}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
