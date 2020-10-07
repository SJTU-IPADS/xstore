#pragma once

#include <gflags/gflags.h>
#include <map>

#include "ycsb.hpp"
#include "nut.hpp"
#include "./osm.hh"

namespace fstore {

namespace server {

enum
{
  YCSB = 0,
  YCSBH,
  DUMMY,
  NONE,
  TPCC,
  NUTO,
  OSM,
};

DECLARE_int64(ycsb_num);

class Loader
{
public:
  using type_map_t = std::map<std::string, int>;
  static type_map_t get_type_map()
  {
    type_map_t loader_mapping = { { "ycsb", YCSB },   { "ycsbh", YCSBH },
                                  { "dummy", DUMMY }, { "tpcc", TPCC },
                                  { "nut0", NUTO },   { "osm", OSM }  };
    return loader_mapping;
  }
};

} // server

} // fstore
