#pragma once

#include <unistd.h>
#include <vector>
#include <string>

#include "./statics.hh"

#include "../deps/r2/src/timer.hh"

namespace xstore {

namespace bench {

template <class T> std::string format_value(T value, int precission = 4) {
  std::stringstream ss;
  ss.imbue(std::locale(""));
  ss << std::fixed << std::setprecision(precission) << value;
  return ss.str();
}

class Reporter {
public:
  static double report_thpt(std::vector<Statics> &statics, int epoches) {

    std::vector<Statics> old_statics(statics.size());

    r2::Timer timer;
    for (int epoch = 0; epoch < epoches; epoch += 1) {
      sleep(1);

      u64 sum = 0;
      // now report the throughput
      for (uint i = 0; i < statics.size(); ++i) {
        auto temp = statics[i].data.counter;
        sum += (temp - old_statics[i].data.counter);
        old_statics[i].data.counter = temp;
      }

      double passed_msec = timer.passed_msec();
      double res = static_cast<double>(sum) / passed_msec * 1000000.0;
      r2::compile_fence();
      timer.reset();

      DISPLAY(2) << "epoch @ " << epoch
                 << ": thpt: " << format_value(res, 0)
                 << " reqs/sec." << passed_msec
                 << " msec passed since last epoch.";
    }
    return 0.0;
  }
};

} // namespace bench

} // namespace xstore