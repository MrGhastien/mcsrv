#ifndef BITWISE_H
#define BITWISE_H

#include "definitions.h"
#include <stddef.h>

#define CONTINUE_BIT 0x80
#define SEGMENT_BITS 0x7F

#define VARINT_MAX_SIZE 4

#define HTON(size) i##size hton##size(i##size x)
#define UHTON(size) u##size uhton##size(u##size x)

#define NTOH(size) i##size ntoh##size(i##size x)
#define UNTOH(size) u##size untoh##size(u##size x)

u64 ceil_two_pow(u64 num);
u64 u64_log2(u64 x);
void* offset(const void* ptr, i64 offset);
void* offsetu(const void* ptr, u64 offset);

static inline ptrdiff_t ptr_diff(const void* a, const void* b) {
    return (const char*)a - (const char*) b;
}

bool is_addr_aligned(void* addr, u64 alignment);

HTON(16);
HTON(32);
HTON(64);
UHTON(16);
UHTON(32);
UHTON(64);

NTOH(16);
NTOH(32);
NTOH(64);
UNTOH(16);
UNTOH(32);
UNTOH(64);

#endif /* ! BITWISE_H */
