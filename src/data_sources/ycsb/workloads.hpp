#pragma once

#include "../../utils/zipfan.hpp"
#include "../workloads.hpp"

#include "hash.hpp"

namespace fstore {

namespace sources {

namespace ycsb {

/*!
  Note, this only generate keys,not workloads.
 */
class YCSBCWorkload : public BaseWorkload<YCSBCWorkload>
{
public:
  YCSBCWorkload(u32 total_records,
                u64 seed = 0xdeadbeaf,
                bool need_hash = false)
    : record_count(total_records)
    , need_hash(need_hash)
    , zip_generator(total_records, seed)
  {}

  YCSBCWorkload(u32 total_records, bool need_hash, u64 seed)
    : record_count(total_records)
    , need_hash(need_hash)
    , zip_generator(total_records, seed)
  {}

  u64 next_key_impl()
  {
    auto key = zip_generator.next();
    if (need_hash)
      key = Hasher::hash(key);
    else {
      key = Hasher::hash(key) % record_count;
    }
    return key;
  }

  utils::ZipFanD zip_generator;
  const u64 record_count = 100000;
  const bool need_hash = false;
};

class YCSBCWorkloadUniform : public BaseWorkload<YCSBCWorkloadUniform>
{
public:
  YCSBCWorkloadUniform(u32 total_records,
                       u64 seed = 0xdeadbeaf,
                       bool need_hash = false)
    : record_count(total_records)
    , need_hash(need_hash)
    , rand(seed)
  {}

  u64 next_key_impl()
  {
#if 1
    u64 key = rand.next() % record_count;
    if (need_hash)
      key = Hasher::hash(key);
    return key;
#else
    return 0;
#endif
  }

  r2::util::FastRandom rand;
  const u64 record_count = 100000;
  const bool need_hash = false;
};

} // end namespace ycsb

}

} // end namespace fstore
