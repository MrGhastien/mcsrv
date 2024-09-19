#ifndef DICT_H
#define DICT_H

#include "definitions.h"
#include "memory/arena.h"
#include "utils/hash.h"

struct node {
    u64 hash;
    u64* hashp;
    void* key;
    void* value;
};

typedef u64 (*hash_function)(const void* key);

typedef struct dict {
    void* base;
    const Comparator* comparator;
    u64 capacity;
    u64 size;
    u64 key_stride;
    u64 value_stride;
    bool fixed; //True if the underlying array cannot be resized.
} Dict;

void dict_init(Dict* map, const Comparator* cmp, u64 key_stride, u64 value_stride);
void dict_init_fixed(Dict* map, const Comparator* cmp, Arena* arena, u64 capacity, u64 key_stride, u64 value_stride);
void dict_clear(Dict* map);
void dict_destroy(Dict* map);

i64 dict_put(Dict* map, const void* key, const void* value);
i64 dict_remove(Dict* map, const void* key, void* outValue);

i64 dict_get(Dict* map, const void* key, void* outValue);
void* dict_ref(Dict* dict, i64 idx);

typedef void (*action)(const Dict* dict, u64 idx, void* key, void* value, void* data);
void dict_foreach(const Dict* map, action action, void* data);

#endif /* ! DICT_H */
