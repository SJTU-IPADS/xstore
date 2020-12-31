#pragma once

#include <functional>
#include <iostream>
#include <string>

#include "../deps/r2/src/common.hh"

/*!
  load keys from a file
 */

namespace xstore {

struct FileLoader
{
  std::ifstream file;

  explicit FileLoader(const std::string& name)
    : file(name)
  {}

  template<typename KeyType>
  auto next_key(std::function<KeyType(const std::string&)> converter)
    -> ::r2::Option<KeyType>
  {
    std::string line;
    if (std::getline(file, line)) {
      return converter(line);
    }
    return {};
  }

  template<typename KeyType>
  static auto default_converter(const std::string& s) -> KeyType
  {
    KeyType ret;
    std::istringstream iss(s);
    iss >> ret;
    return ret;
  }

  ~FileLoader() { file.close(); }
};

} // namespace xstore
