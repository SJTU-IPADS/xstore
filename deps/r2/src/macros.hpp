#pragma once

namespace r2
{

#define DISABLE_COPY_AND_ASSIGN(classname) \
private:                                   \
  classname(const classname &) = delete;   \
  classname &operator=(const classname &) = delete

#define unlikely(x) __builtin_expect(!!(x), 0)
#define likely(x) __builtin_expect(!!(x), 1)

#define NOT_INLE __attribute__((noinline))
#define ALWAYS_INLINE __attribute__((always_inline))

static inline unsigned long
read_tsc(void)
{
  unsigned a, d;
  __asm __volatile("rdtsc"
                   : "=a"(a), "=d"(d));
  return ((unsigned long)a) | (((unsigned long)d) << 32);
}

class RDTSC
{
public:
  RDTSC() : start(read_tsc())
  {
  }
  unsigned long passed() const
  {
    return read_tsc() - start;
  }

private:
  unsigned long start;
};

static inline void compile_fence(void)
{
  asm volatile("" ::
                   : "memory");
}

} // end namespace r2
