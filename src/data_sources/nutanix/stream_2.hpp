#pragma once

/// the genereation are sampled from [0, 500000000) following the distribution
/// `in https://github.com/BLepers/KVell/blob/master/workload-common.c`

namespace fstore {

namespace sources {

// T is the tree from ../stores/naive_tree.hpp which implements get(u64)
// NutOne replay the key distribution of `long production_random1(void)`
template<typename T>
class NutTwo : public datastream::StreamIterator<u64, u64>
{

  const u64 key_space = 500000000L;

  const u64 total_keys;
  u64 current_loaded = 0;
  T* db;

  u64 cur_key = 0;

  r2::util::FastRandom rand;

public:
  NutTwo(T* tree, u64 total_keys, u64 seed = 0xdeadbeaf)
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

    if (prob < 103487) {
      rand_key = rand_key % 47016400;
    } else if (prob < 570480) {
      rand_key = 47016400 + rand_key % (259179450 - 47016400);
    } else if (prob < 849982) {
      rand_key = 259179450 + rand_key % (386162550 - 259179450);
    } else if (prob < 930511) {
      rand_key = 386162550 + rand_key % (422748200 - 386162550);
    } else if (prob < 973234) {
      rand_key = 422748200 + rand_key % (442158000 - 422748200);
    } else if (prob < 986958) {
      rand_key = 442158000 + rand_key % (448392900 - 442158000);
    } else {
      rand_key = 448392900 + rand_key % (500000000 - 448392900);
    }
    return rand_key;
  }
};
}
}
