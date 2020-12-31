#pragma once

#include <string>
#include <vector>

namespace xstore {
namespace util {

template<typename T>
std::string
vec_slice_to_str(const std::vector<T>& v, int begin, int end)
{
  std::ostringstream oss;
  oss << "[";
  for (uint i = begin; i < std::min(static_cast<int>(v.size()), end); ++i)
    oss << v[i] << ",";
  oss << "]";
  return oss.str();
}

}
}