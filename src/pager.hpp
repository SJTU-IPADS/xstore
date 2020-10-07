#pragma once

#include "common.hpp"

#include <tuple>

namespace fstore {

typedef u32 page_id_t;

template <typename Page>
class MallocPager
{
public:
  static Page* allocate_one(){
    auto res = calloc(1,sizeof(Page));
    ASSERT(res != nullptr);
    return (Page *)res;
  };

  static u32 page_id(const Page *) {
    return 0;
  }
};

/**
 * XD: current problem: what if we want kvs with different page sizes?
 * current we cannot support this.
 */
template<typename Page>
class BlockPager
  {
  public:
    /*!
      \param block_page: the pointer points to a block memory, which the
      following page will allocate from it \param sz: the total size of the
      given memory

      \note: this function will align the pointer
    */
    static std::tuple<void*, std::size_t> init(char* blocks_page,
                                               std::size_t sz)
    {
      if (unlikely(blocks == nullptr)) {
        blocks = blocks_page;
        size = sz;
#if 1
      if(!std::align(sizeof(Page),sizeof(Page),blocks,size))
        blocks = nullptr;
      else {
        heap_ptr = blocks;
      }
#endif
    }
    return std::make_tuple(blocks,size);
  }

  static u64 max_pages() {
    return size / sizeof(Page);
  }

  /*!
    allocate one page from the block of memory, return nullptr if there is no remaining free space
  */
  static Page *allocate_one() {
    if(unlikely(size - ((u64)heap_ptr - (u64)blocks) < sizeof(Page))) {
      return nullptr;
    }
    auto res = heap_ptr;
    heap_ptr = (void *)((u64)heap_ptr + sizeof(Page));
    allocated += 1;
    return static_cast<Page *>(res);
  }

  static u32 page_id(const Page *ptr) {
    ASSERT(ptr >= static_cast<Page *>(blocks) && ptr < static_cast<Page *>(heap_ptr));
    auto res =  ((u64)ptr - (u64)blocks) / sizeof(Page);
    return res;
  }

  static Page *get_page_by_id(const u32 id) {
    if(unlikely(id * sizeof(Page) > size)) {
      return nullptr;
    }
    return (Page *)(id * sizeof(Page) + (char *)blocks);
  }

  static u64 page_size() {
    return sizeof(Page);
  }

  static void         *blocks;
  static void         *heap_ptr;
  static std::size_t   size;

  static u64 allocated;
};

template <typename P>
void *BlockPager<P>::blocks = nullptr;
template <typename P>
std::size_t   BlockPager<P>::size   = 0;
template <typename P>
void  *BlockPager<P>::heap_ptr = nullptr;
template <typename P>
u64 BlockPager<P>::allocated = 0;

} // end namespace fstore
