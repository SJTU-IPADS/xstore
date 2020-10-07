#pragma once

#include "./workloads.hpp"

namespace fstore {

namespace sources {

// used to generate key distribution in nutanix workloads
class TXTWorkload : public BaseWorkload<TXTWorkload>
{
public:
  std::vector<u64> *all_keys_vec = nullptr;
  r2::util::FastRandom rand;

  TXTWorkload(std::vector<u64> *keys, u64 seed) : all_keys_vec(keys), rand(seed) {
    ASSERT(all_keys_vec->size() != 0);
  }

  u64 next_key_impl(){
    auto idx = rand.next() % all_keys_vec->size();
    return (*all_keys_vec)[idx];
  }
};

}

}
