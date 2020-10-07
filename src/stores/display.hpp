#pragma once

#include <string>

#include "node.hpp"

namespace fstore {

namespace store {

template <typename Node,template <class> class P>
class Display {
 public:
  static std::string page_to_string(const Node *node) {
    std::ostringstream oss;
    oss << "[ page_id: " << P<Node>::page_id(node) << " | ";
    oss << "keys: " << node->num_keys << " | ";
    if(node->num_keys > 0)
      oss << "from [ " << node->keys[0] << "," << node->keys[node->num_keys - 1] << "]";
    oss << "]";
    return oss.str();
  }
};

}

}
