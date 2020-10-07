#pragma once

#include "naive_tree.hpp"

namespace fstore {

namespace store {

/*!
  A concurrent safe B+tree using HTM (namely, DBX Tree from Eurosys'2014).
  It's just a simple wrapper over a single-threaded tree.
 */
template <typename KEY, typename VAL,template <class> class P = BlockPager >
class DBXTree {
 public:
  using NaiveTree<KEY,VAL,P> BareMetaTree;

  DBXTree() {
    not_implemented();
  }
 private:
  BareMetaTree t;
};

} // store

} // fstore
