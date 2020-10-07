#pragma once

#include "../workloads.hpp"

namespace fstore {

namespace sources {

// used to generate key distribution in nutanix workloads
class Nut0Workload : public BaseWorkload<Nut0Workload>
{
public:
  r2::util::FastRandom rand;
  const u64 max_key_space = 100000;

  Nut0Workload(u64 seed) : rand(seed) {
  }

  u64 next_key_impl() {
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

class Nut1Workload : public BaseWorkload<Nut1Workload>
{
public:
  r2::util::FastRandom rand;
  const u64 max_key_space = 100000;

  Nut1Workload(u64 seed)
    : rand(seed)
  {}

  u64 next_key_impl()
  {
    u64 rand_key = rand.next();
    u64 prob = rand.next() % 10000;
    //u64 prob = rand.next() % 1000000;

    if (prob < 103487)
    {
      rand_key = rand_key % 47016400;
    }
    else if (prob < 570480)
    {
      rand_key = 47016400 + rand_key % (259179450 - 47016400);
    }
    else if (prob < 849982)
    {
      rand_key = 259179450 + rand_key % (386162550 - 259179450);
    }
    else if (prob < 930511)
    {
      rand_key = 386162550 + rand_key % (422748200 - 386162550);
    }
    else if (prob < 973234)
    {
      rand_key = 422748200 + rand_key % (442158000 - 422748200);
    }
    else if (prob < 986958)
    {
      rand_key = 442158000 + rand_key % (448392900 - 442158000);
    }
    else { rand_key = 448392900 + rand_key % (500000000 - 448392900); }

    return rand_key;
  }
};
}
}
