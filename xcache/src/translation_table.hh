#pragma once

#include <vector>

#include "../../deps/r2/src/common.hh"

#include "../../xutils/marshal.hh"

#include "./lib.hh"

namespace xstore {

namespace xcache {

using namespace r2;

// only leaf the 8-bit of the incarnation
const usize kIncarBit = 8;
const usize kIncarMask = bitmask<usize>(kIncarBit);

template<typename EntryType>
struct TT
{

  explicit TT(const ::xstore::string_view& s) { this->from_serialize(s); }

  TT() = default;

  using ET = EntryType;

  std::vector<EntryType> entries;

  EntryType& operator[](int i) { return entries.at(i); }

  auto add(const EntryType& e) { entries.push_back(e); }

  auto get_wo_incar(const usize& i) -> EntryType
  {
    // not implemented
    // 1. retrieve the value
    auto addr = (*this)[i];
    // 2. trim the incarnation
    addr = addr >> kIncarBit;
    return addr;
  }

  auto add_w_incar(const EntryType& e, const usize& incar)
  {
    auto temp = e << kIncarBit;
    ASSERT(temp >> kIncarBit == e);
    temp |= (incar & kIncarMask);
    entries.push_back(temp);
  }

  auto size() const -> usize { return entries.size(); }

  auto mem() const -> usize { return this->size() * sizeof(EntryType); }

  auto clear() { entries.clear(); }

  /*!
    The serialization protocol works as follows:
    | num entries | entry 0 | entry 1|, ....
    num_entries: u32
   */
  auto serialize() const -> std::string
  {
    std::string res;
    res += ::xstore::util::Marshal<u32>::serialize_to(this->size());
    for (uint i = 0; i < this->size(); ++i) {
      res += ::xstore::util::Marshal<EntryType>::serialize_to(entries[i]);
    }
    return res;
  }

  static auto tt_entry_sz() -> usize { return sizeof(EntryType); }

  auto tt_sz() -> usize
  {
    return sizeof(u32) + this->entries.size() * sizeof(EntryType);
  }

  auto from_serialize(const ::xstore::string_view& d)
  {
    char* cur_ptr = (char*)(d.data());
    ASSERT(d.size() >= sizeof(u32));

    u32 n = ::xstore::util::Marshal<u32>::deserialize(cur_ptr, d.size());
    ASSERT(d.size() >= sizeof(u32) + n * sizeof(EntryType))
      << d.size() << "; n:" << n;
    // ASSERT(n < 2048) << n;

    cur_ptr += sizeof(u32);
    this->clear();
    this->entries.resize(n);

    for (uint i = 0; i < n; ++i) {
      this->entries[i] =
        ::xstore::util::Marshal<EntryType>::deserialize(cur_ptr, d.size());
      cur_ptr += sizeof(EntryType);
    }
  }
};
} // namespace xcache
} // namespace xstore
