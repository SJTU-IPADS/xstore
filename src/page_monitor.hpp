#include "pager.hpp"

#include "r2/src/timer.hpp"

namespace fstore {

/*!
  Monitor how many pages has been allocated
 */
template<typename P>
class PageMonitor
{
  u64 previous_alloced = 0;
  r2::Timer timer;
public:
  PageMonitor()
    : previous_alloced(P::allocated)
  {}

  double alloced_thpt()
  {
    auto temp = P::allocated;
    auto passed_msec = timer.passed_msec();

    auto thpt =
      static_cast<double>(temp - previous_alloced) / passed_msec * (1000000.0);
    previous_alloced = temp;
    return thpt;
  }
};
}
