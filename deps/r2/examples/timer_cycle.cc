#include "common.hpp"
#include "timer.hpp"

#include <vector>
#include <cinttypes>
#include <stdio.h>

using namespace r2;
using namespace std;

typedef size_t uint;

template <typename T>
double count_avg(const vector<T> &v) {
  double sum = 0;
  uint64_t count  = 0;
  for(auto i : v) {
    sum += (static_cast<double>(i) - sum) / (++count);
  }
  return sum;
}

int main() {

  vector<uint64_t> counts;
  vector<double>   results;
  Timer t;
  for(uint i = 0;i < 100000;++i) {
    auto start = read_tsc();
    compile_fence();
    results.push_back(t.passed_msec());
    compile_fence();
    auto elapsed = read_tsc() - start;
    counts.push_back(elapsed);
    compile_fence();
    t.reset();
  }

  fprintf(stdout,"avg secs used: %f\n",count_avg(results));
  fprintf(stdout,"avg cycles used: %f\n",count_avg(counts));
  return 0;
}
