#pragma once

#include "common.hpp"
#include "jemalloc/jemalloc.h"

namespace r2
{

using ptr_t = void *;

class Allocator
{
public:
  explicit Allocator(unsigned id) : id(id)
  {
  }

  inline ptr_t alloc(u32 size, int flag = 0)
  {
    auto ptr = jemallocx(size, id | flag);
    return ptr;
  }

  inline ptr_t alloc_large(u64 size, int flag = 0) {
    auto ptr = jemallocx(size, id | flag);
    return ptr;
  }

  inline void dealloc(ptr_t ptr) { free(ptr); }

  inline void free(ptr_t ptr)
  {
    /*!
      According to WenHao, using 0 is fine, because jemalloc will
      automatically free the memory to this thread's allocation.
      But according to my test, if using id has better scalability.d
     */
    //jedallocx(ptr,0);
    jedallocx(ptr, id);
  }

private:
  unsigned id;
};

} // end namespace r2
