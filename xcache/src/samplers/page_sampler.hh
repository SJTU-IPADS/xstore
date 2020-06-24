#pragma once

#include "../sample_trait.hh"

#include "../logic_addr.hh"

namespace xstore {

namespace xcache {

/*!
  page sampler only adds the first and last elements in a B+Tree leaf node to
  the training-set The training-set should be: [key,
  logical_addr_in_one_leaf,key1, logic1, ... ] where the logical address should
  be: logical = logic_node_id * N + offset; where N is the maxinum number of
  keys in one leaf node.

  The logical address format encode/decode is defined in ../logic_addr.hh

  The unit test file is in ../../tests/test_sampler.cc
 */

struct PageSampler : public SampleTrait<PageSampler> {
  const usize N;
  Option<u64> initial_logic_addr = {};

  // current iterated key
  Option<KeyType> iter_key = {};
  Option<u64> iter_logic_addr = {};

  explicit PageSampler(const usize &N) : N(N) {}

  auto add_to_impl(const KeyType &k, const u64 &l, std::vector<KeyType> &t_set,
                   std::vector<u64> &l_set) {
    if (!initial_logic_addr) {
      // add
      t_set.push_back(k);
      l_set.push_back(l);

      initial_logic_addr = l;
    } else {
      // check if current iter_logic_addr is the last one
      if (LogicAddr::decode_logic_id(l) !=
          LogicAddr::decode_logic_id(initial_logic_addr.value())) {
        // add the previous storing iter_logic_addr
        auto init = initial_logic_addr.value();
        auto end = iter_logic_addr.value();
        ASSERT(LogicAddr::decode_logic_id(init) ==
               LogicAddr::decode_logic_id(end));

        if (LogicAddr::decode_off(init) != LogicAddr::decode_off(end)) {
          // add if different
          t_set.push_back(iter_key.value());
          l_set.push_back(end);
        }

        // re-set
        initial_logic_addr = l;
        iter_key = {};
        iter_logic_addr = {};

        t_set.push_back(k);
        l_set.push_back(l);

      } else {
        // this key can be temporally not added to the training-set
        // record it in iter_key, iter_logic_addr
        iter_key = k;
        iter_logic_addr = l;
      }
    }
  }

  auto finalize_impl(std::vector<KeyType> &t_set, std::vector<u64> &l_set) {
    if (iter_key) {
      t_set.push_back(iter_key.value());
      l_set.push_back(iter_logic_addr.value());
    }
  }
};

} // namespace xcache
} // namespace xstore
