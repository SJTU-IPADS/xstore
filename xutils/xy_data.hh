#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "../deps/r2/src/common.hh"

namespace xstore {

namespace util {

/*!
  The data structure for manipulating x -> data.
  Record and output them to a python readable format.
 */
template<typename K, typename V>
struct XYData
{
  std::vector<std::pair<K, V>> data;

  XYData() = default;

  auto add(const K& k, const V& v) -> XYData&
  {
    data.push_back(std::make_pair(k, v));
    return *this;
  }

  auto finalize() -> XYData&
  {
    using T = std::pair<K, V>;
    std::sort(this->data.begin(),
              this->data.end(),
              [](const T& lhs, const T& rhs) { return lhs.first < rhs.first; });

    return *this;
  }

  auto dump_as_np_data() -> std::string
  {
    std::ostringstream osx;
    osx << "X = [";
    std::ostringstream osy;
    osy << "Y = [";
    for (uint i = 0; i < this->data.size(); i++) {
      osx << std::get<0>(this->data.at(i)) << ",";
      osy << std::get<1>(this->data.at(i)) << ",";
    }
    osx << "]";
    osy << "]";
    osx << std::endl << osy.str();
    return osx.str();
  }

  auto dump_as_np_data(const std::string& file)
  {
    auto data = this->dump_as_np_data();
    std::ofstream out(file);
    out << data;
    out.close();
  }
};

} //

}