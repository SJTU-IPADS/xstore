#pragma once

#include "common.hpp"
#include "utils/all.hpp"

#include <algorithm>

namespace fstore {

#pragma pack(1)
struct ModelDesc {
  u64 num_entries;
  u64 entry_size;
};

class DefaultAllocator {
 public:
  char *alloc(u64 size) {
    return (char *)malloc(size);
  }
};

class ModelDescGenerator {
 public:
  using buf_desc_t = std::tuple<char *,u64>;
  using buf_vec_t = std::vector<buf_desc_t>;

  template <class Allocator = DefaultAllocator>
  static buf_vec_t generate_two_stage_desc(
      Allocator &alloc,
      const std::vector<std::string> &first,
      const std::vector<std::string> &second) {
    buf_vec_t res;
    res.push_back(generate_one_stage_dec<Allocator>(alloc,first));
    res.push_back(generate_one_stage_dec<Allocator>(alloc, second));
    return res;
  }

  template <class Allocator = DefaultAllocator>
  static buf_desc_t generate_mega_buf(
      Allocator &alloc,
      const std::string &res) {
    char *buf = (char *)alloc.alloc(res.size() + sizeof(u64));
    memcpy(buf,res.data(),res.size());
    return std::make_pair(buf,res.size());
  }

  template <class Allocator = DefaultAllocator>
  static buf_desc_t generate_one_stage_dec(Allocator &alloc,const std::vector<std::string> &first) {

    // first we calculate the max size of each entry
    u64 max_entry_sz = 0;
    for(auto &s :first)
      max_entry_sz = std::max(max_entry_sz,static_cast<u64>(s.size()));
    max_entry_sz = utils::round_up<u64>(max_entry_sz,sizeof(u64));

    char *buf = (char *)alloc.alloc(sizeof(ModelDesc) + max_entry_sz * first.size());
    ASSERT(buf != nullptr);
    *((ModelDesc *)buf) = {.num_entries = first.size(), .entry_size = max_entry_sz};

    // iteratively copy the buf to one buf
    char *cur_ptr = buf + sizeof(ModelDesc);
    for(auto &s : first) {
      memcpy(cur_ptr,s.data(),s.size());
      cur_ptr += max_entry_sz;
    }
    return std::make_tuple(buf,sizeof(ModelDesc) + max_entry_sz * first.size());
  }

  /*!
    Warning ! we donot do sanity check for the buf, so this call may be **Very** dangerous.
   */
  static std::vector<std::string>
  deserialize_one_stage(char *buf) {
    std::vector<std::string> res;
    ModelDesc *desc = (ModelDesc *)buf;

    // simple sanity checks of the buffer
    //ASSERT(desc->entry_size < 4096);
    //ASSERT(desc->num_entries < 4096);

    char *cur_ptr = buf + sizeof(ModelDesc);
    for(uint i = 0;i < desc->num_entries;++i) {
      res.emplace_back(cur_ptr,desc->entry_size);
      cur_ptr += desc->entry_size;
    }
    return res;
  }
};

} // fstore
