#pragma once

namespace fstore {

namespace sources {

namespace  ycsb {

/*!
  Hash the key to another space: make it *non linear*
  Credits:  https://github.com/basicthinker/YCSB-C
 */
class Hasher {

  static const u64 kFNVOffsetBasis64 = 0xCBF29CE484222325;
  static const u64 kFNVPrime64       = 1099511628211;

 public:
  static inline u64 FNVHash64(u64 val) {
    u64 hash = kFNVOffsetBasis64;

    for (int i = 0; i < 8; i++) {
      u64 octet = val & 0x00ff;
      val = val >> 8;

      hash = hash ^ octet;
      hash = hash * kFNVPrime64;
    }
    return hash;
  }

  static inline u64 hash(u64 val) {
    return FNVHash64(val) >> 32;
  }
};

} // end namespace ycsb

}

} // fstore
