#pragma once

#include "common.hpp"
#include "flags.hpp"

namespace fstore {

namespace bench {

enum MonitorRPCID
{
  START = 4,
  END,
  PING,
  BEAT
};

const int controler_id = 75;

struct Reports
{
  double throughpt = 0.0;
  double other    = 0.0;
  double other1    = 0.0;
  double other2    = 0.0;
  double latency = 0.0;
};

} // bench

} // fstore
