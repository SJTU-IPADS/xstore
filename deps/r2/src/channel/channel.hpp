#pragma once

#include "../common.hpp"

#include <stdlib.h>

namespace r2
{

/*!
  TODO: add an example here
 */
template <class T>
class Channel
{
public:
  Channel(u64 max_entry_num = 1)
      : max_entry_num(max_entry_num), head(0), tail(0)
  {
    ASSERT(!(max_entry_num & (max_entry_num - 1)));

    ring_buf = static_cast<Entry *>(
        ::aligned_alloc(kCacheLineSize, max_entry_num * sizeof(Entry)));
    ASSERT(ring_buf != nullptr);
    ASSERT((u64)ring_buf % ENTRY_SIZE == 0);
  }

  ~Channel() { free(ring_buf); }

  inline u64 size() const { return head - tail; }

  inline bool isEmpty() const { return head == tail; }

  inline bool enqueue(const T &value)
  {
    ASSERT(head >= tail);
    if (head == tail + max_entry_num)
    {
      return false;
    }
    else
    {
      u64 index = head & (max_entry_num - 1);

      ring_buf[index].value = value;
      __sync_fetch_and_add(&head, 1);
      return true;
    }
  }

  inline void enqueue_blocking(const T &value)
  {
    ASSERT(head >= tail);
    while (head == tail + max_entry_num)
      ;
    u64 index = head & (max_entry_num - 1);

    ring_buf[index].value = value;
    __sync_fetch_and_add(&head, 1);
  }

  inline T dequeue_blocking()
  {
    ASSERT(head >= tail);
    while (head == tail)
      ;
    u64 index = tail & (max_entry_num - 1);

    T ret = ring_buf[index].value;
    __sync_fetch_and_add(&tail, 1);
    return ret;
  }

  inline Option<T> dequeue()
  {
    ASSERT(head >= tail);
    if (head == tail)
    {
      return {};
    }
    else
    {
      u64 index = tail & (max_entry_num - 1);
      auto res = Option<T>(ring_buf[index].value);
      ::r2::compile_fence();
      __sync_fetch_and_add(&tail, 1);
      return res;
    }
  }

private:
  static const u64 ENTRY_SIZE = kCacheLineSize;

  struct Entry
  {
    T value;
  } __attribute__((aligned(ENTRY_SIZE)));

  // XD: why public?
public:
  volatile u64 __attribute__((aligned(kCacheLineSize))) head;
  volatile u64 __attribute__((aligned(kCacheLineSize))) tail;
  Entry *ring_buf;
  u64 max_entry_num;
} __attribute__((aligned(kCacheLineSize)));

} // end namespace r2
