#pragma once

#include "../../deps/r2/src/common.hh"
#include "../../deps/r2/src/random.hh"

#include <algorithm> // generate_n
#include <random>
#include <string>

namespace test {

using namespace r2;

static ::r2::util::FastRandom rand(0xdeadbeaf);

template <usize N> struct __attribute__((packed))  TestObj {
  char data[N];
  u64 checksum = 0;
  inline auto sz() -> usize { return N; }
} __attribute__((aligned(sizeof(u64))));

// random string generator from
// https://stackoverflow.com/questions/440133/how-do-i-create-a-random-alpha-numeric-string-in-c
void inplace_rand_str(char *buf, size_t len) {
  auto randchar = []() -> char {
    const char charset[] = "0123456789"
                           "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                           "abcdefghijklmnopqrstuvwxyz";
    const size_t max_index = (sizeof(charset) - 1);
    return charset[rand.next() % max_index];
  };
  std::generate_n(buf, len, randchar);
}

auto random_string(size_t length) -> std::string {
  std::string str(length, 0);
  inplace_rand_str((char *)str.data(), length);
  return str;
}

auto simple_checksum(const char *buf, size_t len) -> usize {
  usize sum = 0;
  for (uint i = 0;i < len; ++i) {
    sum += static_cast<usize>(buf[i]);
  }
  return ~sum;
}

auto verbose_simple_checksum(const char *buf, size_t len) -> usize {
  LOG(4) << "start check sum with len: " << len;
  usize sum = 0;
  for (uint i = 0; i < len; ++i) {
    LOG(4) << "add: " << static_cast<usize>(buf[i]);
    sum += static_cast<usize>(buf[i]);
  }
  LOG(4) << "done checksum: " << sum << " ------ " << ~sum;
  return ~sum;
}

} // namespace test
