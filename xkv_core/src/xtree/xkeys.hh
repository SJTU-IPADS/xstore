#pragma once

#include <algorithm>
#include <limits>

// offsetof
#include <cstddef>

#include "../../../deps/r2/src/common.hh"

namespace xstore {

namespace xkv {

namespace xtree {

using namespace r2;

/*!
  We assume that the key cannot be kInvalidKey.
  This key is served as a placeholder for checking whether a key is present.
 */
const u64 kInvalidKey = std::numeric_limits<u64>::max();

using IncarType = u16;

template <usize N, typename K> struct __attribute__((packed)) XNodeKeys {
  static_assert(std::numeric_limits<u8>::max() >= N,
                "The number of keys is too large for XNode");
  K keys[N];
  IncarType incarnation = 0;

  XNodeKeys() {
    for (uint i = 0; i < N; ++i) {
      this->keys[i].from_u64(kInvalidKey);
    }
  }

  /*!
    \ret: the index installed
   */
  auto add_key(const K &key) -> ::r2::Option<u8> {
    for (uint i = 0; i < N; ++i) {
      if (this->keys[i] == K(kInvalidKey)) {
        this->keys[i] = key;
        return i;
      }
    }
    return {};
  }

  auto search(const K &key) -> ::r2::Option<u8> {
    for (uint i = 0; i < N; ++i) {
      if (this->keys[i] == key) {
        return i;
      }
    }
    return {};
  }

  auto num_keys() -> usize {
    usize sum = 0;
    for (uint i = 0; i < N; ++i) {
      if (this->keys[i] != K(kInvalidKey)) {
        sum += 1;
      }
    }
    return sum;
  }

  auto max_key() -> K {
    auto temp = K::min();
    for (uint i = 0; i < N; ++i) {
      if (this->keys[i] != K(kInvalidKey) && this->keys[i] > temp) {
        temp = this->keys[i];
      }
      //LOG(4) << this->keys[i] << " " << temp;
    }
    //LOG(4) << "done:" << temp;
    return temp;
  }

  auto get_key(const int &idx) const -> K { return keys[idx]; }

  void clear(const int &idx) { this->keys[idx] = K(kInvalidKey); }

  /*!
    memory offset of keys entry *idx*
  */
  auto key_offset(const int &idx) -> usize {
    using Self = XNodeKeys<N, K>;
    return offsetof(Self, keys) + sizeof(K) * idx;
  }

  auto find_median_key() -> ::r2::Option<u8> {
    std::vector<std::pair<K, int>> temp;
    for (uint i = 0; i < N; ++i) {
      temp.push_back(std::make_pair(this->keys[i], i));
    }

    if (temp.size() > 0) {
      std::sort(temp.begin(), temp.end(), [](auto &left, auto &right) {
        return left.first < right.first;
      });
      return std::get<1>(temp[temp.size() / 2]);
    }
    return {};
  }
};

} // namespace xtree

} // namespace xkv

} // namespace xstore
