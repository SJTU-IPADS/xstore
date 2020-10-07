#pragma once

#include "./sub_model.hh"

#include "../marshal.hpp"

#include "../utils/all.hpp"

namespace fstore {

#define EXT 1

#pragma pack(1)
struct SubHeader {
  u32 page_entries;
  u32 model_sz;
#if EXT
  u64 ext_page_addr;
#endif
  u32 train_seq;
};

  const usize max_page_entries_to_serialize = 4;

static_assert(max_page_entries_to_serialize <= max_page_table_entry, "");

class Serializer {

public:
  template<typename S>
  static usize direct_serialize(S& model, char* buf_to_serialize){
    auto buf = model.ml.serialize();

    SubHeader header
    {
      .page_entries =
#if !EXT
        model.page_table.size() > max_page_entries_to_serialize
          ? max_page_entries_to_serialize
          : model.page_table.size(),
#else
        model.page_table.size(),
#endif
      .model_sz = buf.size(),
#if EXT
      .ext_page_addr = (u64)(0),
#endif
      .train_seq = model.train_watermark
    };

    char* header_buf = buf_to_serialize;
    char* ptr = Marshal<SubHeader>::serialize_to(header, header_buf);

    // then the ml
    memcpy(ptr, buf.data(), buf.size());
    ptr += buf.size();

    // then the error
    ptr = Marshal<i32>::serialize_to(model.min_error, ptr);
    ptr = Marshal<i32>::serialize_to(model.max_error, ptr);
    ptr = Marshal<i64>::serialize_to(static_cast<u64>(model.max_entries), ptr);

    for (uint i = 0; i < header.page_entries; ++i) {
      if (ptr - buf_to_serialize + sizeof(u64) > 4000) {
        return 0;
      }
      auto e = model.page_table[i];
      ptr = Marshal<u64>::serialize_to(e, ptr);
    }

    return ptr - buf_to_serialize;
  }

  template<typename S>
  static void serialize_submodel(S& model, char* buf_to_serialize)
  {
    // then the model
    auto buf = model.ml.serialize();
#if EXT
    char *ext_addr = nullptr;
    if (model.page_table.size() > max_page_entries_to_serialize) {
      auto allocator = r2::AllocatorMaster<0>::get_thread_allocator();
      ext_addr = (char *)allocator->alloc(model.page_table.size() * sizeof(u64));
      ASSERT(ext_addr != nullptr);
    }
#endif
    //ASSERT(ext_addr != nullptr);
    *((u64 *)buf_to_serialize) = 73; // invalid the buf
    r2::compile_fence();

    // first serialize the header
    SubHeader header{ .page_entries =
#if !EXT
        model.page_table.size() > max_page_entries_to_serialize
        ? max_page_entries_to_serialize
        : model.page_table.size(),
#else
                      model.page_table.size(),
#endif
                      .model_sz = buf.size(),
#if EXT
                      .ext_page_addr = (u64)(ext_addr),
#endif
                      .train_seq = model.train_watermark
    };

#if !EXT
    if (model.page_table.size() > max_page_entries_to_serialize) {
      LOG(4) << "warnining: model page table entries too much: " << model.page_table.size()
             << "; check error: " << model.max_error - model.min_error << "; entries:" << model.max_entries;
    }
#endif
    char *header_buf = buf_to_serialize + sizeof(u64);
    char* ptr = Marshal<SubHeader>::serialize_to(header, header_buf);

    // then the ml
    memcpy(ptr, buf.data(), buf.size());
    ptr += buf.size();

    // then the error
    ptr = Marshal<i32>::serialize_to(model.min_error, ptr);
    ptr = Marshal<i32>::serialize_to(model.max_error, ptr);
    ptr = Marshal<i64>::serialize_to(static_cast<u64>(model.max_entries), ptr);

#if !EXT
    if (model.page_table.size() > max_page_entries_to_serialize) {
      //ASSERT(false);
      return;
    }

    // finially the page entries
    for (uint i = 0; i < header.page_entries; ++i) {
      auto e = model.page_table[i];
      ptr = Marshal<u64>::serialize_to(e, ptr);
    }
#else
    if (ext_addr != nullptr) {
      ptr = ext_addr;
    }
    for (uint i = 0; i < header.page_entries; ++i) {
      auto e = model.page_table[i];
      ptr = Marshal<u64>::serialize_to(e, ptr);
    }
#endif

    r2::compile_fence();
    ASSERT(*((u64 *)buf_to_serialize) = 73);
    *((u64*)buf_to_serialize) = 1; // invalid the buf
    r2::compile_fence();
    *((u64*)ptr) = 1;
    r2::compile_fence();
    // done
  }

  template <typename S>
  static S extract_submodel(char *buf) {
    S submodel;
    extract_submodel_to(buf, submodel);
    return submodel;
  }

  template<typename S>
  static bool direct_extract(char *buf, S &submodel) {
    SubHeader header = Marshal<SubHeader>::extract_with_inc(buf);
    if (header.model_sz == 0) {
      LOG(4) << "page entries: " << header.page_entries;
      return false;
    }
    if (header.page_entries == 0) {
      LOG(4) << "page entries 0 !";
      // meet a zero model
      return false;
    }

    submodel.ml.from_serialize(std::string(buf, header.model_sz));
    submodel.train_watermark = header.train_seq;

    buf += header.model_sz;

    //submodel.page_table.clear();
    if (header.page_entries > submodel.page_table.size()) {
      submodel.page_table.resize(header.page_entries);
    }

    // min max entries
    submodel.min_error = Marshal<i32>::extract_with_inc(buf);
    submodel.max_error = Marshal<i32>::extract_with_inc(buf);
    submodel.max_entries = Marshal<i64>::extract_with_inc(buf);

    // now serialize the page table
    for (uint i = 0; i < header.page_entries; ++i) {
      auto addr = Marshal<u64>::extract_with_inc(buf);
      if (unlikely(addr == INVALID_PAGE_ID)) {
        return false;
      }
      submodel.page_table[i] = addr;
    }

    return true;

  }

  template<typename S>
  static bool extract_submodel_to(char* buf, S &submodel, Option<u64> &ext_addr)
  {
    u64 seq = *((u64 *)buf);
    buf += sizeof(u64);
    //ASSERT(seq == 1) << "extract seq: " << seq;
    SubHeader header = Marshal<SubHeader>::extract_with_inc(buf);
    if (header.model_sz == 0) {
      LOG(4) << "page entries: " << header.page_entries;
      ASSERT(false) << "seq: " << header.train_seq << " " << seq;
      return false;
    }
    if (header.page_entries == 0) {
      // meet a zero model
      return true;
    }

    submodel.ml.from_serialize(std::string(buf, header.model_sz));
    submodel.train_watermark = header.train_seq;

    buf += header.model_sz;

    //submodel.page_table.clear();
    if (header.page_entries > submodel.page_table.size()) {
      submodel.page_table.resize(header.page_entries);
    }

    // min max entries
    submodel.min_error = Marshal<i32>::extract_with_inc(buf);
    submodel.max_error = Marshal<i32>::extract_with_inc(buf);
    submodel.max_entries = Marshal<i64>::extract_with_inc(buf);

#if !EXT
    ASSERT(header.page_entries <= max_page_entries_to_serialize);
#endif

    //LOG(4) << "serailize : " << header.page_entries << " ;max: " << max_page_entries_to_serialize;
    if (header.page_entries <= max_page_entries_to_serialize) {
      // now serialize the page table
      for (uint i = 0; i < header.page_entries; ++i) {
        auto addr = Marshal<u64>::extract_with_inc(buf);
        // submodel.page_table.push_back(addr);
        // ASSERT(addr != INVALID_PAGE_ID) << " at entry:" << i;
        if (unlikely(addr == INVALID_PAGE_ID)) {
#if EXT
          //ASSERT(false);
#endif
          return false;
        }
        submodel.page_table[i] = addr;
      }
    } else {
      //LOG(4) << "set header: " << header.page_entries << " " << header.ext_page_addr;
      submodel.page_table.resize(header.page_entries);
      ext_addr = header.ext_page_addr;
    }
    return true;
  }

  template<typename T>
  static usize sizeof_submodel()
  {
    return ::fstore::utils::round_up(sizeof(SubHeader) + T::buf_sz() +
                                       max_page_entries_to_serialize *
                                         sizeof(u64) +
                                       sizeof(i32) * 2 + sizeof(i64),
                                     64lu); // cacheline
  }

  template<typename T>
  static usize sizeof_model()
  {
    return ::fstore::utils::round_up(sizeof(SubHeader) + T::buf_sz() +
                                       sizeof(i32) * 2 + sizeof(i64),
                                     64lu); // cacheline
  }
};

}
