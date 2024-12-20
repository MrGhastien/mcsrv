#ifndef BITWISE_H
#define BITWISE_H

#include "definitions.h"

#define CONTINUE_BIT 0x80
#define SEGMENT_BITS 0x7F

#define VARINT_MAX_SIZE 4

u64 ceil_two_pow(u64 num);
void* offset(void* ptr, i64 offset);
void* offsetu(void* ptr, u64 offset);

u64 hton64(u64 x);
u64 ntoh64(u64 x);

i16 big_endian16(i16 x);
i32 big_endian32(i32 x);
i64 big_endian64(i64 x);

#endif /* ! BITWISE_H */
