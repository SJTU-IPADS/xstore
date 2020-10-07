#pragma once

/// the genereation are sampled from [0, 500000000) following the distribution
/// `in https://github.com/BLepers/KVell/blob/master/workload-common.c`

namespace fstore {

namespace sources {

const u64 key_space = 500000000L;

// T is the tree from ../stores/naive_tree.hpp which implements get(u64)
// NutOne replay the key distribution of `long production_random1(void)`
template<typename T>
class NutOne : public datastream::StreamIterator<u64, u64>
{
  const u64 total_keys;
  u64 current_loaded = 0;
  T* db;

  u64 cur_key = 0;

  r2::util::FastRandom rand;

public:
  NutOne(T* tree, u64 total_keys, u64 seed = 0xdeadbeaf)
    : total_keys(total_keys)
    , rand(seed)
    , db(tree)
  {
    next();
  }

  void begin() override { current_loaded = 0; }

  bool valid() override { return current_loaded < total_keys; }

  void next() override
  {
  retry:
    auto tentative_key = roll_one();
    if (db->get(tentative_key) != nullptr)
      goto retry;
    cur_key = tentative_key;
  }

  u64 key() override
  {
    current_loaded += 1;
    return cur_key;
  }

  u64 value() override
  {
    return 0; // not supported
  }

  u64 roll_one()
  {
    u64 rand_key = rand.next();
    u64 prob = rand.next() % 10000;

    if (prob < 13) {
      rand_key = 0 + rand_key % 144000000;
    } else if (prob < 8130) {
      rand_key = 144000000 + rand_key % (314400000 - 144000000);
    } else if (prob < 9444) {
      rand_key = 314400000 + rand_key % (450000000 - 314400000);
    } else if (prob < 9742) {
      rand_key = 450000000 + rand_key % (480000000 - 450000000);
    } else if (prob < 9920) {
      rand_key = 480000000 + rand_key % (490000000 - 480000000);
    } else {
      rand_key = 490000000 + rand_key % (500000000 - 490000000);
    }
    return rand_key;
  }
};
}
}
