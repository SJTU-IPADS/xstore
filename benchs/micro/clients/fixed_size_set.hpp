#pragma once

#include <vector>

// private usage
namespace fstore {

// a wrapper over vector, which can store at most N keys

template<int N>
struct FixedVector
{
  std::vector<u64> data;

  FixedVector() {
    data.reserve(N);
  }

  void emplace(u64 key) {
    if (data.size() == N) {
      // delete half the data
      data.erase(data.begin(), data.begin() + N / 2);
    }
    data.push_back(key);
  }

  u64 get_latest(u64 zipfan_n) {
    assert(data.size() != 0);
    if (zipfan_n >= data.size()) {
      // return the last element
      return data[data.size() - 1];
    }
    return data[data.size() - zipfan_n - 1];
  }
};

}
