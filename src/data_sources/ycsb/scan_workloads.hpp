#pragma once

#include "../../common.hpp"

#include "../../utils/zipfan.hpp"

#include "hash.hpp"

namespace fstore {

namespace sources {

namespace ycsb {
class YCSBScanWorkload
{
public:
  /*!
    Most of consts definations come from
    https://github.com/brianfrankcooper/YCSB/blob/9566b70832a1d9a3af7db84a42e0d9873c5d910a/core/src/main/java/com/yahoo/ycsb/workloads/CoreWorkload.java

    Note: the scan range distribution can be either uniform/zipfan.

    For simplicity, we choose the Zipfan distribution to generate scan key

    Note: if we want to evaluate non-exsisting keys, we should pass num_keys as
    the maxinum key
   */

  explicit YCSBScanWorkload(u64 key_space,
                            u64 seed = 0xdeadbeaf,
                            bool need_hash = true)
    : key_space(key_space)
    , need_hash(need_hash)
    , rand(seed)
  {}

  /*!
    (seek_key, batch_num)
   */
  using scan_range = std::pair<u64, u16>;

  scan_range next_range()
  {
    u16 num = static_cast<u16>(rand.next() % (max_scan_range - min_scan_range) + 1);
    ASSERT(num > 0);
    u64 start = rand.next() % key_space;
    return std::make_pair(start,num);
  }

  const u16 min_scan_range = 1;

  const u16 max_scan_range = 100;
  /*
  Maybe we should use 100? Masstree[Eurosys'12] uses 1-100 scan range.
   */
  const u64 key_space;
  const bool need_hash = true;

  r2::util::FastRandom rand;
};

} // namespace ycsb

} // namespace sources

} // namespace fstore
