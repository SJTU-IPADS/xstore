#pragma once

#include <string>
#include <unistd.h>
#include <vector>

#include <iomanip> // std::setprecision

#include "./statics.hh"

#include "../deps/r2/src/timer.hh"

namespace xstore {

namespace bench {

template<class T>
std::string
format_value(T value, int precission = 4)
{
  std::stringstream ss;
  ss.imbue(std::locale(""));
  ss << std::fixed << std::setprecision(precission) << value;
  return ss.str();
}

class Reporter
{
public:
  static double report_thpt(std::vector<Statics>& statics,
                            int epoches,
                            ::r2::Option<std::string> log_file = {})
  {

    std::vector<Statics> old_statics(statics.size());

    std::ofstream outfile;
    if (log_file) {
      outfile.open(log_file.value(), std::ios::out | std::ios::trunc);
    }

    r2::Timer timer;
    for (int epoch = 0; epoch < epoches; epoch += 1) {
      sleep(1);

      u64 sum = 0;
      u64 sum1 = 0;

      // now report the throughput
      for (uint i = 0; i < statics.size(); ++i) {
        auto temp = statics[i].data.counter;
        sum += (temp - old_statics[i].data.counter);
        old_statics[i].data.counter = temp;

        temp = statics[i].data.counter1;
        sum1 += (temp - old_statics[i].data.counter1);
        old_statics[i].data.counter1 = temp;

        // lat
      }

      double passed_msec = timer.passed_msec();
      double res = static_cast<double>(sum) / passed_msec * 1000000.0;
      double res1 = static_cast<double>(sum1) / passed_msec * 1000000.0;
      r2::compile_fence();
      timer.reset();

      LOG(2) << "epoch @ " << epoch << ": thpt: " << format_value(res, 0)
             << " reqs/sec;"
             << "second: " << format_value(res1, 0) << " reqs/sec"
             << "; lat: " << statics[0].data.lat << " us";

      if (log_file) {
        outfile << format_value(res, 0) << "\n";
      }
    }
    if (log_file) {
      outfile.close();
    }
    return 0.0;
  }
};

} // namespace bench

} // namespace xstore
