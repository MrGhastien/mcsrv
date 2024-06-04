#ifndef BITWISE_H
#define BITWISE_H

#include "bitwise.h"

size_t ceil_two_pow(size_t num) {
    num |= num >> 1;
    num |= num >> 2;
    num |= num >> 4;
    num |= num >> 8;
    num |= num >> 16;
    num |= num >> 32;
    return num + 1;
}

#endif /* ! BITWISE_H */
