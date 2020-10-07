#pragma once

#include "common.hpp"
#include "utils/all.hpp"

#include "cpptoml/include/cpptoml.h"

namespace fstore {

namespace server {

using namespace ::fstore::utils;

/*!
  Load the configuration file of f-store server.
 */
class Config {
 public:
  u64 page_mem_sz = 1 * GB;
  u64 rdma_heap_sz = 1 * GB;
  std::string msg_type = "ud";

  using config_handle_t =  std::shared_ptr<cpptoml::table>;
  Config(const std::string &f) : Config(cpptoml::parse_file(f)) {
  }

  Config(const config_handle_t &handler) :
      page_mem_sz(parse_mem_sz(handler->get_qualified_as<std::string>("memory.page").value_or("2GB"))),
      rdma_heap_sz(parse_mem_sz(handler->get_qualified_as<std::string>("memory.rdma_heap").value_or("1GB"))),
      msg_type(handler->get_qualified_as<std::string>("rpc.network").value_or("ud")) {
    ASSERT(handler->get_qualified_as<std::string>("memory.page"));
  }

  u64 total_mem_sz() const {
    return page_mem_sz + rdma_heap_sz;
  }

  std::string to_str() const {
    std::ostringstream oss; oss << "Server config: \n";
    oss << "\tusing memory for leaf nodes: " <<
        utils::format_value(utils::bytes2G(page_mem_sz,true)) << " GB;\n";
    oss << "\tallocated RDMA heap size: "
        << utils::format_value(utils::bytes2G(rdma_heap_sz,true)) << " GB;\n";
    oss << "\tserver communication type: " << msg_type << ".\n";
    return oss.str();
  }

 private:
  static u64 parse_mem_sz(const std::string &s) {
    u64 num = std::stoi(s);
    if(s.find_first_of("G") != std::string::npos)
      num *= GB;
    else if(s.find_first_of("M") != std::string::npos)
      num *= MB;
    else if(s.find_first_of("K") != std::string::npos)
      num *= KB;
    return num;
  }
};

} // namespace server

} //namespace fstore
