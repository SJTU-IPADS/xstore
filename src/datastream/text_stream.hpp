#pragma once

#include "stream.hpp"
#include "common.hpp"

#include <fstream>
#include <string>
#include <tuple>

namespace fstore {

namespace datastream {

/*!
  The stream comes from iterating lines from a .txt file.
  The ParseF(const std::string &line) -> KeyType,ValType parse the given line and returns the (key,value)
 */
template <typename KeyType, typename V,std::tuple<KeyType,V> (*ParseF)(const std::string &)>
class TextIter : public StreamIterator<KeyType,V> {
 public:
  TextIter(const std::string &file_name) : file(file_name) {
    begin();
  }

  ~TextIter() {
    file.close();
  }

  void begin() override {
    file.seekg (0, file.beg);
    next();
  }

  bool valid() override {
    return line_valid;
  }

  void next() override {
    line_valid = !(!std::getline(file,cur_line));
  }

  KeyType key() override {
    return std::get<0>(ParseF(cur_line));
  }

  V value() override {
    return std::get<1>(ParseF(cur_line));
  }

 private:
  std::ifstream file;
  std::string   cur_line;
  bool          line_valid = false;
};

template <typename KeyType, typename V,std::tuple<KeyType,V> (*ParseF)(const std::string &)>
class NumTextIter : public StreamIterator<KeyType,V> {
 public:
  NumTextIter(const std::string &file_name,u64 num) : core(file_name),total(num) {
    core.begin();
  }

  void begin() override {
    core.begin();
  }

  bool valid() override {
    return core.valid() && total > cur;
  }

  void next() override {
    cur += 1;
    core.next();
  }

  KeyType key() override {
    return core.key();
  }

  V value() override {
    return core.value();
  }

 private:
  TextIter<KeyType,V,ParseF> core;
  u64 cur = 0;
  u64 total = 0;
};

} // datastream

} // fstore
