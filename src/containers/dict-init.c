#include "containers/vector.h"
#include "dict.h"
#include "logger.h"
#include "memory/_memory_internal.h"
#include "memory/allocators/buddy.h"
#include "memory/allocators/pool.h"
#include "memory/mem_tags.h"
#include "platform/platform.h"
#include "utils/hash.h"

#include <stdlib.h>
#include <string.h>

#define DEFAULT_CAP 4

void dict_init_fixed(
    Dict* map, const Comparator* cmp, u64 capacity, u64 key_stride, u64 value_stride) {

    // Check if capacity is null or not a power of two
    if (capacity == 0 || (capacity & (capacity - 1)) != 0) {
        log_fatalf("Dictionnary capacity is not a power of two (%lu)", capacity);
        platform_abort();
    }

    map->comparator   = cmp;
    map->key_stride   = key_stride;
    map->value_stride = value_stride;
    map->capacity     = capacity;
    map->size         = 0;
    map->fixed        = TRUE;

    pool_init(&map->values, capacity, value_stride, BLK_TAG_UNKNOWN, INVALID_CHAIN);
    pool_init(&map->keys, capacity, key_stride, BLK_TAG_UNKNOWN, INVALID_CHAIN);
    buddy_init(&map->entries, 1 << 16, BLK_TAG_UNKNOWN);
    map->base = buddy_alloc(&map->entries, sizeof *map->base * capacity);
    memset(map->base, 0, sizeof *map->base * capacity);
}

void dict_init(Dict* map, const Comparator* cmp, u64 key_stride, u64 value_stride) {
    map->comparator   = cmp;
    map->key_stride   = key_stride;
    map->value_stride = value_stride;
    map->capacity     = DEFAULT_CAP;
    map->size         = 0;
    map->fixed        = FALSE;

    pool_init_dynamic(&map->values, DEFAULT_CAP, value_stride, BLK_TAG_UNKNOWN, INVALID_CHAIN);
    pool_init_dynamic(&map->keys, DEFAULT_CAP, key_stride, BLK_TAG_UNKNOWN, INVALID_CHAIN);
    buddy_init(&map->entries, 1 << 16, BLK_TAG_UNKNOWN);
    map->base = buddy_alloc(&map->entries, sizeof *map->base * DEFAULT_CAP);
    memset(map->base, 0, sizeof *map->base * DEFAULT_CAP);
}

void dict_destroy(Dict* map) {
    pool_destroy(&map->values);
    pool_destroy(&map->keys);
    buddy_destroy(&map->entries);
}
