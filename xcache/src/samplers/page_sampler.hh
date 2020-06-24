#pragma once

#include "../sample_trait.hh"

namespace xstore {

namespace xcache {

/*!
  page sampler only adds the first and last elements in a B+Tree leaf node to
  the training-set The training-set should be: [key,
  logical_addr_in_one_leaf,key1, logic1, ... ] where the logical address should
  be: logical = logic_node_id * N + offset; where N is the maxinum number of
  keys in one leaf node.

  The logical address format encode/decode is defined in ../logic_addr.hh
 */

struct PageSampler : public SampleTrait<PageSampler> {
  const usize N;
  const Option<usize> initial_logic_addr = {};

  // current iterated key
  const Option<KeyType> iter_key = {};
  const Option<usize> iter_logic_addr = {};

  explicit PageSampler(const usize &N) : N(N) {}

  auto add_to_impl(const KeyType &k, const u64 &l, std::vector<KeyType> &t_set,
                   std::vector<u64> &l_set) {
    if (!initial_logic_addr) {
      // add
      t_set.push_back(k);
      l_set.push_back(l);

      initial_logic_addr = l;
    } else {

    }
  }
};

} // namespace xcache
} // namespace xstore
