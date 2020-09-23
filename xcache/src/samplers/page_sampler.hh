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

template <usize N, typename KeyType>
struct PageSampler : public SampleTrait<PageSampler<N,KeyType>,KeyType> {
  ::r2::Option<KeyType> min_key_in_page = {};
  ::r2::Option<u64> min_key_label = {};

  ::r2::Option<KeyType> max_key_in_page = {};
  ::r2::Option<u64> max_key_label = {};

  PageSampler() = default;

  /*!
    \note: keys in one page may be not sorted, so we explicitly sort them
    // l: the label
   */
  auto add_to_impl(const KeyType &k, const u64 &l, std::vector<KeyType> &t_set,
                   std::vector<u64> &l_set) {
    if (!min_key_in_page) {
      // the start case
      this->min_key_in_page = k;
      this->min_key_label = l;
      return;
    }

    // iterating
    // 1. check whether we enter the next page?
    if (LogicAddr::decode_logic_id<N>(l) !=
        LogicAddr::decode_logic_id<N>(this->min_key_label.value())) {
      // first add the current
      this->add_cur(t_set, l_set);

      // reset
      this->min_key_in_page = k;
      this->min_key_label = l;
      this->max_key_label = {};
      this->max_key_in_page = {};
    } else {
      // we only need to update the min_max in this case
      if (!this->max_key_in_page || k > this->max_key_in_page.value()) {
        this->max_key_in_page = k;
        this->max_key_label = l;
      }
      if (this->min_key_in_page.value() > k) {
        this->min_key_in_page = k;
        this->min_key_label = l;
      }
    }
  }

  auto add_cur(std::vector<KeyType> &t_set,std::vector<u64> &l_set) {

    if (this->min_key_in_page) {
      t_set.push_back(this->min_key_in_page.value());
      l_set.push_back(this->min_key_label.value());
    }
    if (this->max_key_in_page) {
      // they must be in the same leaf node!
      ASSERT(LogicAddr::decode_logic_id<N>(this->max_key_label.value()) ==
             LogicAddr::decode_logic_id<N>(this->min_key_label.value()));
      if (LogicAddr::decode_off<N>(this->min_key_label.value()) !=
          LogicAddr::decode_off<N>(this->max_key_label.value())) {
        // add because they have different offset
        t_set.push_back(this->max_key_in_page.value());
        l_set.push_back(this->max_key_label.value());
      }
    }
  }

  auto finalize_impl(std::vector<KeyType> &t_set, std::vector<u64> &l_set) {
    this->add_cur(t_set, l_set);
  }
};

} // namespace xcache
} // namespace xstore
