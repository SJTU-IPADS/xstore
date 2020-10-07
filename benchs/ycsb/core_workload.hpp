#pragma once

#include "r2/src/random.hpp"

#include <functional>
#include <string>

namespace fstore {

namespace bench {

  using txn_fn_t = std::function<u64(handler_t& h, char*, u64)>;

struct WorkloadDesc
{
  std::string name;
  double frequency;
  txn_fn_t fn;
  u64 executed = 0;
};

class Workload
{
public:
  explicit Workload(u64 seed)
    : rand(seed)
  {}

  std::pair<size_t, u64> run(std::vector<WorkloadDesc>& workload,
                             handler_t& h,
                             char* buf, u64 k)
  {

    auto d = rand.next_uniform();

    int idx = 0;
    for (size_t i = 0; i < workload.size(); ++i) {
      if ((i + 1) == workload.size() || d < workload[i].frequency) {
        idx = i;
        break;
      }
      d -= workload[i].frequency;
    }
    workload[idx].executed += 1;
    return std::make_pair(idx, workload[idx].fn(h, buf,k));
  }

  r2::util::FastRandom rand;
};

} // bench

} // fstore
