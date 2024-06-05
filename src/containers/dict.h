#ifndef DICT_H
#define DICT_H

#include "../definitions.h"
#include "../memory/arena.h"

#include <stddef.h>

struct node {
    u64 hash;
    u64* hashp;
    void* key;
    void* value;
};

typedef struct map {
    Arena* arena;
    void* base;
    size_t key_stride;
    size_t value_stride;
    size_t capacity;
    size_t size;
    bool external_arena;
} Dict;

void dict_init(Dict* map, size_t key_stride, size_t value_stride);
void dict_init_with_arena(Dict* map, Arena* arena, size_t stride);
void dict_clear(Dict* map);
void dict_destroy(Dict* map);
void dict_deep_destroy(Dict* map);

#define dict_put_imm(map, key, value, type)                                                                            \
    {                                                                                                                  \
        assert(sizeof(type) == (map)->stride);                                                                         \
        type holder = value;                                                                                           \
        dict_put(map, key, &holder);                                                                                   \
    }

void dict_put(Dict* map, void* key, void* value);
bool dict_remove(Dict* map, void* key, void* outValue);

bool dict_get(Dict* map, void* key, void* outValue);

typedef void (*action)(const Dict* dict, void* key, void* value, void* data);
void dict_foreach(Dict* map, action action, void* data);

#endif /* ! DICT_H */
