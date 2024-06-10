#ifndef BITWISE_H
#define BITWISE_H

#include "../definitions.h"
#include <stddef.h>

size_t ceil_two_pow(size_t num);
void* offset(void* ptr, size_t offset);

u64 hton64(u64 x);
u64 ntoh64(u64 x);

#endif /* ! BITWISE_H */
