#include "dict.h"
#include "utils/hash.h"

#include <stdlib.h>

#define DEFAULT_CAP 4

void dict_init_fixed(Dict* map, const Comparator* cmp, Arena* arena, u64 capacity, u64 key_stride, u64 value_stride) {
    map->base = arena_callocate(arena, capacity * (sizeof(u64) + key_stride + value_stride));
    map->comparator = cmp;
    map->key_stride = key_stride;
    map->value_stride = value_stride;
    map->capacity = capacity;
    map->size = 0;
    map->fixed = TRUE;
}

void dict_init(Dict* map, const Comparator* cmp, u64 key_stride, u64 value_stride) {
    map->base = calloc(DEFAULT_CAP, sizeof(u64) + key_stride + value_stride);
    map->comparator = cmp;
    map->key_stride = key_stride;
    map->value_stride = value_stride;
    map->capacity = DEFAULT_CAP;
    map->size = 0;
    map->fixed = FALSE;
}

void dict_destroy(Dict* map) {
    if (!map->fixed)
        free(map->base);
}
