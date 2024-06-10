#include "dict.h"

#include <stdlib.h>

#define DEFAULT_CAP 4

void dict_init_fixed(Dict* map, Arena* arena, size_t capacity, size_t key_stride, size_t value_stride) {
    map->base = arena_callocate(arena, capacity * (sizeof(u64) + key_stride + value_stride));
    map->key_stride = key_stride;
    map->value_stride = value_stride;
    map->capacity = capacity;
    map->fixed = TRUE;
    map->size = 0;
}

void dict_init(Dict* map, size_t key_stride, size_t value_stride) {
    map->base = calloc(DEFAULT_CAP, sizeof(u64) + key_stride + value_stride);
    map->key_stride = key_stride;
    map->value_stride = value_stride;
    map->capacity = DEFAULT_CAP;
    map->fixed = FALSE;
    map->size = 0;
}

void dict_destroy(Dict* map) {
    if (!map->fixed)
        free(map->base);
}
