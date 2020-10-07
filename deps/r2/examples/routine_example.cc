#include "rexecutor.hpp"

using namespace r2;

int main() {

  RExecutor<> r;
  for(uint i = 0;i < 12;++i) {
    r.spawn<RExecutor<>>([i](handler_t &h,RExecutor<> &2) {
                           LOG(2) << "routine: " << r.cur_id() << " started";
                           r.yield_to_next(h);
                           LOG(4) << "routine: " << r.cur_id() << " yield back";
                           routine_ret(h,r);
                         });
  }
  r.run();
}
