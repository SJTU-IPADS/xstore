#pragma once

#include "common.hpp"
#include "r2/src/allocator_master.hpp"
#include "utils/memory_util.hpp"

#include <functional>
#include <iterator>
#include <map>
#include <sstream>
#include <vector>

namespace fstore {

using namespace ::fstore::utils;
using namespace r2;

/*!
  Region defines the memory layout specified by the user.
  A user can create a region with the following property:
  - name: the name of the region,
  - addr: the virtual address of the memory at local machine
  - base: the base memory related to xxx.
 */
class RegionDesc
{
public:
  RegionDesc(const std::string& n, uintptr_t addr, u64 size, uintptr_t base)
    : name(n)
    , addr(addr)
    , size(size)
    , base(base)
  {}

  const std::string name;
  const uintptr_t addr;
  const u64 size;
  const uintptr_t base;

  friend std::ostream& operator<<(std::ostream& os, const RegionDesc& s)
  {
    auto off = s.addr - s.base;
    auto suffix = s.size >= GB ? "GB" : (s.size >= MB ? "MB" : "KB");

    return os << s.name << " "                  // the name
              << std::hex << "0x" << off << ":" // the start address
              << std::hex << "0x" << off        // the end address
              << "+" << utils::bytes2G(s.size, true) << suffix;
  }

  char* get_as_virtual() const { return reinterpret_cast<char*>(addr); }

  // base offset of this region
  u64 get_as_rdma() const { return addr - base; }
};

class RegionManager
{
  typedef std::function<void*(size_t)> Allocator;

public:
  explicit RegionManager(u64 size, Allocator alloc = malloc)
    : base_mem_( size != 0 ? static_cast<char*>(alloc(size)) : nullptr)
    , base_size_(size)
    , memory_(reinterpret_cast<uintptr_t>(base_mem_))
    , size_(size)
  {
    LOG(4) << "region manager alloc memory: " << (void *)base_mem_;
    ASSERT(memory_ != 0);
  }

  explicit RegionManager(char* mem, u64 size)
    : base_mem_(mem)
    , base_size_(size)
    , memory_(reinterpret_cast<uintptr_t>(base_mem_))
    , size_(size)
  {}

  Option<u64> get_rdma_addr(char* buf) const
  {
    if (buf < base_mem_ || (buf > (base_mem_ + base_size_))) {
      // ASSERT(false) << "buf: " << (void *)buf << ";"
      //<< "base:" << (void *)base_mem_;
      return {};
    }
    return Option<u64>{ static_cast<u64>(buf - base_mem_) };
  }

  uintptr_t get_startaddr() const
  {
    if (memory_ == 0) {
      ASSERT(is_heapize());
      return regions_[0].addr;
    }
    return memory_;
  }

  // FIXME: we should delete such memory
  ~RegionManager() = default;

  /*!
    whether a heap is built from this manager.
   */
  bool is_heapize() const { return memory_ == 0; }

  /**
   * Get a specific describtion of the region
   */
  RegionDesc get_region(const std::string& n) const
  {
    if (region_id_map_.find(n) != region_id_map_.end()) {
      return get_region(region_id_map_.find(n)->second);
    } else
      return get_region(-1);
  }

  RegionDesc get_region(int idx) const
  {
    if (idx >= 0 && idx < regions_.size())
      return regions_[idx];
    return RegionDesc("Invalid",
                      reinterpret_cast<uintptr_t>(nullptr),
                      0,
                      reinterpret_cast<uintptr_t>(nullptr));
  }

  /*!
   * Alloc a region specificed by name and size.
   * \return: an ID points. returns -1 if failed to allocate.
   */
  int register_region(const std::string& name, u64 size)
  {
    if (region_id_map_.find(name) != region_id_map_.end()) {
      LOG(4) << name << " has been registered.";
      return -1; // name has already been registered
    }

    if (size > size_)
      return -2; // no free memory left

    regions_.emplace_back(
      name, memory_, size, reinterpret_cast<uintptr_t>(base_mem_));
    region_id_map_.insert(std::make_pair(name, regions_.size() - 1));

    memory_ += size;
    size_ -= size;
    return regions_.size() - 1;
  }

  // use the remaining memory as an RDMA heap
  int make_rdma_heap()
  {
    if (size_ < MIN_HEAP_SIZE) {
      LOG(4) << "too little space left for the heap: " << size_
             << " bytes; requires at least " << MIN_HEAP_SIZE << " MB.";
      return -3; // heap minimal memory not enough
    }

    // call Rmalloc to alloca the heap meta data
    AllocatorMaster<>::init((char*)memory_, size_);

    auto ret = register_region(HEAP_NAME, size_);

    // after making a heap, no free memory left
    memory_ = 0;
    return ret;
  }

  std::string to_str() const
  {
    std::ostringstream oss;
    oss << "| ";
    std::copy(regions_.begin(),
              regions_.end(),
              std::ostream_iterator<RegionDesc>(oss, " | "));
    if (!is_heapize())
      oss << RegionDesc(
               "Unused", memory_, size_, reinterpret_cast<uintptr_t>(base_mem_))
          << " ||";
    return oss.str();
  }

  const char* base_mem_ = nullptr;
  const u64 base_size_ = 0;

private:
  static const constexpr u64 MIN_HEAP_SIZE = 16 * MB;
  const char* HEAP_NAME = "Heap";

  // regions to be alloced
  uintptr_t memory_ = 0;
  u64 size_ = 0;

  std::vector<RegionDesc> regions_;
  std::map<std::string, int> region_id_map_;

  DISABLE_COPY_AND_ASSIGN(RegionManager);
};

} // end namespace fstore
