#pragma once

#include <vector>

namespace r2 {

template <typename T>
class Future {
 public:
  explicit Future(int cid) : cor_id(cid) {
  }

  virtual T   poll(std::vector<int> &routine_count) = 0;
  const   int cor_id;
};

}
