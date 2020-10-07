#pragma once

#include "../common.hpp"

#include <algorithm>
#include <climits>
#include <iomanip>
#include <locale>
#include <map>
#include <sstream>
#include <vector>

#include <iostream>

namespace fstore {

namespace utils {

/*!
  A set of number utilities.
 */
/**
 * This nice code comes from
 * https://stackoverflow.com/questions/1392059/algorithm-to-generate-bit-mask
 */
template<typename R>
static constexpr R
bitmask(unsigned int const onecount)
{
  return static_cast<R>(-(onecount != 0)) &
         (static_cast<R>(-1) >> ((sizeof(R) * CHAR_BIT) - onecount));
}

/*!
  \Note: we assume multiple is power 2. Otherwise, this function is undefined.
 */
template<typename T>
static constexpr T
round_up(const T& num, const T& multiple)
{
  assert(multiple && ((multiple & (multiple - 1)) == 0));
  return (num + multiple - 1) & -multiple;
}

/*!
  A set of display utilites to transform data structure to printable strings.
 */
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

template<class T>
std::string
format_value(T value, int precission = 4)
{
  std::stringstream ss;
  ss.imbue(std::locale(""));
  ss << std::fixed << std::setprecision(precission) << value;
  return ss.str();
}

template<typename T, typename V>
std::string
map_to_str(const std::map<T, V>& m)
{

  std::ostringstream oss;
  oss << "[";
  int num = 0;

  for (auto it = m.begin(); it != m.end(); ++it, num++) {
    oss << "(" << it->first << "," << it->second << ")";
    if (num < m.size() - 1)
      oss << ",";
  }
  oss << "]";
  return oss.str();
}

/*!
  \param: progress, [0,1]
 */
template<typename T>
static inline void
print_progress(double progress, const T& t)
{
  int barWidth = 70;
  std::cout << "[";
  int pos = barWidth * progress;
  for (int i = 0; i < barWidth; ++i) {
    if (i < pos)
      std::cout << "=";
    else if (i == pos)
      std::cout << ">";
    else
      std::cout << " ";
  }
  std::cout << "] " << int(progress * 100.0) << "%"
            << "|" << t << "|"
            << " \r";
  std::cout.flush();
  if (progress == 1.0)
    std::cout << std::endl;
}

} // util

} // fstore
