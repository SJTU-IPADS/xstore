#pragma once

#include "../sample_trait.hh"

namespace xstore {

namespace xcache {

/*!
  Step samplers provides a sampler for adding data to training set in steps.
  For example, if the training data is [0,1,2,4], and the StepSampler is initialized with step 2,
  then it will init the training-set to [0,2].
 */
template<typename KeyType>
struct StepSampler : public SampleTrait<StepSampler<KeyType>,KeyType> {
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

  auto finalize_impl(std::vector<KeyType> &t_set, std::vector<u64> &l_set){
    // do nothing
  }
};

} // namespace xcache
} // namespace xstore
