#include "dict.h"
#include "logger.h"
#include "platform/platform.h"
#include "utils/hash.h"
#include "memory/mem_tags.h"

#include <stdlib.h>

#define DEFAULT_CAP 4

void dict_init_fixed(Dict* map, const Comparator* cmp, Arena* arena, u64 capacity, u64 key_stride, u64 value_stride) {

    // Check if capacity is null or not a power of two
    if (capacity == 0 || (capacity & (capacity - 1)) != 0) {
        log_fatalf("Dictionnary capacity is not a power of two (%lu)", capacity);
        platform_abort();
    }

    map->base = arena_callocate(
        arena, capacity * (sizeof(u64) + key_stride + value_stride)/* , ALLOC_TAG_DICT */);
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
