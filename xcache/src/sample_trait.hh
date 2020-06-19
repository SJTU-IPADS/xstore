#pragma once

#include <vector>

#include "../../x_ml/src/lib.hh"
#include "../../xkv_core/src/lib.hh"

namespace xstore {

namespace xcache {

using namespace xstore::xml;
using namespace xstore::xkv;

/*!
  The sample trait defines how we decide whether we will at one training
  data to the training-set and training label.
 */
template <class Derived> struct SampleTrait {
  auto add_to(const KeyType &k, const u64 &l, std::vector<KeyType> &t_set,
              std::vector<u64> &l_set) {
    return reinterpret_cast<Derived *>(this)->add_to_impl(k, l, t_set, l_set);
  }
};

/*!
  The default sample method will add all K,L to the training-set
 */
struct DefaultSample : public SampleTrait<DefaultSample> {
  auto add_to(const KeyType &k, const u64 &l, std::vector<KeyType> &t_set,
              std::vector<u64> &l_set) {
    t_set.push_back(k);
    l_set.push_back(l);
  }
};

} // namespace xcache
} // namespace xstore
