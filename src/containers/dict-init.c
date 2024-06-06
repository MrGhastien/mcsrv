#include "dict.h"

#include <stdlib.h>
#include <string.h>

#define DEFAULT_CAP 4

void dict_init_with_arena(Dict* map, Arena* arena, size_t key_stride, size_t value_stride) {
    map->capacity = DEFAULT_CAP;
    map->size = 0;
    map->arena = arena;
    size_t total_stride = sizeof(u64) + key_stride + value_stride;
    map->base = arena_allocate(map->arena, total_stride * map->capacity);
    map->key_stride = key_stride;
    map->value_stride = value_stride;
    map->external_arena = TRUE;
}

void dict_init(Dict* map, size_t key_stride, size_t value_stride) {
   Arena* arena = malloc(sizeof(Arena));
    if (!arena)
        return;
    *arena = arena_create();
    dict_init_with_arena(map, arena, key_stride, value_stride);
    map->external_arena = FALSE;
}

void dict_destroy(Dict* map) {
    if (!map->external_arena) {
        arena_destroy(map->arena);
        free(map->arena);
    } else {
        arena_free(map->arena, map->capacity * (sizeof(u64) + map->key_stride + map->value_stride));
    }
}
