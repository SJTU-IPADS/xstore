#pragma once

#include "../sample_trait.hh"

namespace xstore {

namespace xcache {

struct StepSampler : public SampleTrait<StepSampler> {
  const usize step = 1;
  usize cur_count = 0;

  explicit StepSampler(const usize &s) : step(s) {}

  auto add_to_impl(const KeyType &k, const u64 &l, std::vector<KeyType> &t_set,
                   std::vector<u64> &l_set) {
    if (cur_count == 0) {
      // add
      t_set.push_back(k);
      l_set.push_back(l);
    }
    cur_count += 1;

    if (cur_count == step) {
      cur_count = 0;
    }
  }
};

} // namespace xcache
} // namespace xstore
