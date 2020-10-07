#pragma once

#include <map>
#include <sstream>
#include <limits>

namespace fstore {

namespace utils {

template <typename T>
struct NumericReport {
  T      max;
  T      min;
  double average;
};

/*!
  A simple wrapper over std::map, which output the cdf to different formats.
 */
template <typename K,typename V>
class DataMap {
 public:
  typedef std::map<K,V> map_data_t;

  DataMap(const std::string &data_name) : name(data_name) {
  }

  V& operator[](int i) {
    return data[i];
  }

  void insert(const K &k,const V&v) {
    data.insert(std::make_pair(k,v));
  }

  u64 size() const {
    return data.size();
  }

  bool has_key(const K &k) const {
    return data.find(k) != data.end();
  }

  NumericReport<V> report() const {
    u64 count(1);
    double average(0);
    V   min(std::numeric_limits<V>::max()), max(std::numeric_limits<V>::min());

    for(auto it = data.begin();it != data.end();++it,count += 1) {
      min = std::min(min,it->second);
      max = std::max(max,it->second);
      average += (it->second - average) / count;
    }

    return { .min = min,
             .max = max,
             .average = average };
  }

  /*!
    Dump the whole DataMap as the python list format.
    The python list format are [(k0,v0),(k1,v1), ... ,(kn,vn)].
    \return the formated string which can be evaluated by the python interpreter.
   */
  std::string dump_as_python_list() const {
    std::ostringstream oss; oss << "[";
    for(auto it = data.begin(); it != data.end();++it)
      oss << "(" << it->first << "," << it->second << "),";
    oss << "]";
    return oss.str();
  }

  /*!
    Dump the whole DataMap as the python numpy format, which has the following format:
    X = [k0,k1,...,kn]
    Y = [v0,v1,...,vn]
  */
  std::string dump_as_np_data(const std::string &ylabel = "Y",
                              const std::string &xlabel = "X") const {
    std::ostringstream osx; osx << "X = [";
    std::ostringstream osy; osy << "Y = [";
    for(auto it = data.begin(); it != data.end();++it) {
      osx << it->first << ",";
      osy << it->second << ",";
    }
    osx << "]"; osy << "]";
    osx << std::endl << osy.str();
    osx << std::endl << "title = \"" << name  << "\"" << std::endl;
    osx << "ylabel = \"" << ylabel << "\"" << std::endl;
    osx << "xlabel = \"" << xlabel << "\"" << std::endl;
    return osx.str();
  }

  /*!
    Dump the whole DataMap as gnuplot data file.
    A typical gnuplot data file has the following format:
    """
     k0 v0
     k1 v1
     ...
     kn vn
    """
    \return the formated string which can be evaluated by the gnuplot interpreter.
  */
  std::string dump_as_gnuplot_data() const {
    std::ostringstream oss;
    for(auto it = data.begin();it != data.end();++it)
      oss << it->first << " " << it->second << std::endl;
    return oss.str();
  }
 public:
  const std::string name;
 private:
  map_data_t data;
};

}

}
