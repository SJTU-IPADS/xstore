#pragma once

#include <stdint.h>

namespace xstore {

namespace util {

#ifndef xglue
#define xglue(a, b) a##b
#endif
#ifndef glue
#define glue(a, b) xglue(a, b)
#endif

#ifndef LOCK_PREFIX
#define LOCK_PREFIX "lock; "
#endif

#define xstr(s) #s
#define sstr(s) xstr(s)

// Is this the correct way to detect 64 system?
#if (__LP64__ == 1)
/* Compares the value in rdx:rax to value in memp, if equal load rcx:rbx into
 * memp, else load value into rdx:rax.
 * Return 1 on success, 0 otherwise. */
static __inline__ uint8_t atomic_cmpxchg16b(uint64_t *memp, uint64_t old0,
                                            uint64_t old1, uint64_t new0,
                                            uint64_t new1) {
  uint8_t z;
  asm volatile("lock; cmpxchg16b %3\n\t"
               "setz %2\n\t"
               : "+a"(old0), "+d"(old1), "=r"(z), "+m"(*memp)
               : "b"(new0), "c"(new1)
               : "memory", "cc");
  return z;
}
#endif

static __inline__ uint8_t atomic_cmpxchg8b(uint32_t *memp, uint32_t old0,
                                           uint32_t old1, uint32_t new0,
                                           uint32_t new1) {
  uint8_t z;
  asm volatile("lock; cmpxchg8b %3\n\t"
               "setz %2\n\t"
               : "+a"(old0), "+d"(old1), "=r"(z), "+m"(*memp)
               : "b"(new0), "c"(new1)
               : "memory", "cc");
  return z;
}

#define DATA_BITS 8
#include "./atomic-template.h"

#define DATA_BITS 16
#include "./atomic-template.h"

#define DATA_BITS 32
#include "./atomic-template.h"

#define DATA_BITS 64
#include "./atomic-template.h"

} // namespace xutil

} // namespace xstore
