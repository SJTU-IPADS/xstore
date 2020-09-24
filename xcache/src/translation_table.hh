#pragma once

#include <vector>

#include "../../deps/r2/src/common.hh"

#include "../../xutils/marshal.hh"

namespace xstore {

namespace xcache {

using namespace r2;

template <typename EntryType> struct TT {

  explicit TT(const std::string &s) { this->from_serialize(s); }

  TT() = default;

  std::vector<EntryType> entries;

  EntryType &operator[](int i) { return entries.at(i); }

  auto add(const EntryType &e) { entries.push_back(e); }

  auto size() const -> usize { return entries.size(); }

  auto mem() const -> usize { return this->size() * sizeof(EntryType); }

  auto clear() { entries.clear(); }

  /*!
    The serialization protocol works as follows:
    | num entries | entry 0 | entry 1|, ....
    num_entries: u32
   */
  auto serialize() -> std::string {
    std::string res;
    res += ::xstore::util::Marshal<u32>::serialize_to(this->size());
    for (uint i = 0;i < this->size(); ++i) {
      res += ::xstore::util::Marshal<EntryType>::serialize_to(entries[i]);
    }
    return res;
  }

  auto from_serialize(const std::string &d) {
    char *cur_ptr = (char *)(d.data());
    ASSERT(d.size() >= sizeof(u32));

    u32 n = ::xstore::util::Marshal<u32>::deserialize(cur_ptr, d.size());
    ASSERT(d.size() >= sizeof(u32) + n * sizeof(EntryType)) << d.size() << "; n:" << n;

    cur_ptr += sizeof(u32);
    this->clear();
    for (uint i = 0; i < n; ++i) {
      this->add(::xstore::util::Marshal<EntryType>::deserialize(cur_ptr, d.size()));
      cur_ptr += sizeof(EntryType);
    }
  }
};
} // namespace xcache
} // namespace xstore
