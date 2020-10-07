#pragma once

#include "../common.hpp"
#include "common_util.hpp"

#if __APPLE__
#elif __linux
#include <sys/sysinfo.h>
#include <unistd.h>
#include <sys/mman.h>
#endif

namespace fstore {

namespace utils {

/*!
  Some constants for print memory information more pretty.
  !*/
const u64 GB = 1 << 30;
const u64 MB = 1 << 20;
const u64 KB = 1 << 10;

const std::string Gname = "GB";
const std::string Mname = "MB";
const std::string Kname = "KB";

#if __linux
inline void *alloc_huge_page(u64 size,u64 huge_page_sz,bool force = true) {
  auto real_sz = round_up(size,huge_page_sz);
  if(force) {
    char *ptr = (char *)mmap(nullptr, real_sz, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS |
                             MAP_POPULATE | MAP_HUGETLB, -1, 0);
    if (ptr == MAP_FAILED) {
      // The mmap() call failed. Try the malloc instead
      LOG(4) << "huge page alloc failed!";
      goto ALLOC_FAILED;
    } else {
      LOG(4) << "Map hugepage success.";
      return ptr;
    }
  }
ALLOC_FAILED:
  LOG(4) << "use default malloc for allocating page";
  return malloc(real_sz);
}
#else
inline void *alloc_huge_page(u64 size,u64 huge_page_sz,bool force = true) {
  LOG(4) << "Huge page allocation is not supported on this platform, use malloc instead.";
  return malloc(size);
}
#endif

inline double bytes2K(u64 bytes) {
  return bytes / static_cast<double>(KB);
}

inline double bytes2M(u64 bytes,bool downgrade = false) {
  if(downgrade && (double)bytes < MB)
    return bytes2K(bytes);
  return bytes / static_cast<double>(MB);
}

inline double bytes2G(u64 bytes,bool downgrade = false) {
  if(downgrade && (double)bytes < GB)
    return bytes2M(bytes);
  return bytes / static_cast<double>(GB);
}

}

} // end namespace fstore
