#ifndef HASH_H
#define HASH_H

#include "definitions.h"

typedef u64 (*hash_function)(const void* data);
typedef i32 (*comparison_function)(const void* lhs, const void* rhs);

typedef struct {
    hash_function hfunc;
    comparison_function comp;
} Comparator;

u64 default_hash(const void* data, u64 size);
i32 default_cmp(const void* lhs, const void* rhs, u64 size);

u64 cmp_hash(const Comparator* comparator, const void* data, u64 size);
u64 cmp_compare(const Comparator* comparator, const void* lhs, const void* rhs, u64 size);

#endif /* ! HASH_H */
