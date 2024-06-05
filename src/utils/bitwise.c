#ifndef BITWISE_H
#define BITWISE_H

#include "bitwise.h"

#include <stddef.h>

size_t ceil_two_pow(size_t num) {
    num |= num >> 1;
    num |= num >> 2;
    num |= num >> 4;
    num |= num >> 8;
    num |= num >> 16;
    num |= num >> 32;
    return num + 1;
}

void* offset(void* ptr, size_t offset) {
    size_t a = (size_t)ptr;
    a += offset;
    return (void *)a;
}

#endif /* ! BITWISE_H */
