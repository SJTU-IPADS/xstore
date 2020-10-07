#pragma once

#include "../internal/db_traits.hpp"

namespace fstore {

namespace server {
class OSMLoader
{
public:
  static int populuate(Tree& t, int num, const std::string &file, u64 seed)
  {
    int loaded = 0;

    std::ifstream ifs(file);

    std::vector<u64> keys;

    std::string line;
    while (std::getline(ifs, line)) {

      // convert to u64
      u64 key;
      std::istringstream iss(line);
      iss >> key;
      //LOG(4) << " load key: " << key; sleep(1);
      //keys.push_back(key);

      ValType val;
      val.set_meta(key);

      t.put(key, val);

      loaded += 1;
      if (loaded >= num) {
        break;
      }

    }
#if 0
    std::sort(keys.begin(), keys.end());
    for (auto key : keys) {
      ValType val;
      val.set_meta(key);

      t.put(key, val);
    }
#endif
    return loaded;
  }
};
}

}
