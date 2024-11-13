#include "hash.h"
#include <string.h>

// Hash function used in sdbm
// Adapted to work on arbitrary data
u64 default_hash(const void* data, u64 size) {
    const u8* bytes = data;
    u64 hash = 37;
    u8 b;

    for (u64 i = 0; i < size; i++) {
        b = bytes[i];
        hash = b + (hash << 6) + (hash << 16) - hash;
    }

    return hash;
}

i32 default_cmp(const void* lhs, const void* rhs, u64 size) {
    return memcmp(lhs, rhs, size);
}

u64 cmp_hash(const Comparator* comparator, const void* data, u64 size) {
    if(!comparator)
        return default_hash(data, size);
    return comparator->hfunc(data);
}

u64 cmp_compare(const Comparator* comparator, const void* lhs, const void* rhs, u64 size) {
    if(!comparator)
        return default_cmp(lhs, rhs, size);
    return comparator->comp(lhs, rhs);
}
