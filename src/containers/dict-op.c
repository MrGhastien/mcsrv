#include "dict.h"
#include "memory/allocators/buddy.h"
#include "memory/allocators/pool.h"
#include "utils/hash.h"

#include <string.h>

#define MAX_LOAD_FACTOR 0.75
#define MIN_LOAD_FACTOR 0.25
#define MIN_CAPACITY 4

static u64 idx_iter(u64 cap, u64 idx) {
    return (5 * idx + 1) % cap;
}

static i64 find_entry(const Dict* map, const void* key, Entry** out_entry) {
    u64 h     = cmp_hash(map->comparator, key, map->key_stride);
    u64 idx   = h % map->capacity;
    Entry* e  = &map->base[idx];
    u64 count = 0;

    while (!e->key || e->hash != h || cmp_compare(map->comparator, e->key, key, map->key_stride)) {
        if (e->key)
            count++;
        if (count == map->size)
            return -1;
        idx = idx_iter(map->capacity, idx);
        e   = &map->base[idx];
    }
    *out_entry = e;
    return idx;
}

static void rehash_entries(Dict* map, Entry* old_base, Entry* new_base, u64 new_capacity) {
    for (u64 i = 0; i < map->capacity; i++) {
        Entry* entry = &old_base[i];
        if (!entry->key)
            continue;

        u64 ni           = entry->hash % new_capacity;
        Entry* new_entry = &new_base[ni];
        while (new_entry->key) {
            ni        = idx_iter(new_capacity, ni);
            new_entry = &new_base[ni];
        }
        *new_entry = *entry;
    }
}

static void resize(Dict* map, u64 new_capacity) {
    Entry* new_base = buddy_alloc(&map->entries, sizeof * new_base * new_capacity);
    Entry* old_base = map->base;

    memset(new_base, 0, sizeof *new_base * new_capacity);
    rehash_entries(map, old_base, new_base, new_capacity);

    buddy_free(&map->entries, old_base);
    map->capacity = new_capacity;
    map->base     = new_base;
}

static void grow(Dict* map) {
    resize(map, map->capacity << 1);
}

static void shrink(Dict* map) {
    if (map->capacity > MIN_CAPACITY)
        resize(map, map->capacity >> 1);
}

i64 dict_put(Dict* map, const void* key, const void* value) {
    if (key == NULL || map->size == map->capacity)
        return -1;

    if (!map->fixed && map->size >= map->capacity * MAX_LOAD_FACTOR)
        grow(map);

    u64 h        = cmp_hash(map->comparator, key, map->key_stride);
    u64 idx      = h % map->capacity;
    Entry* entry = &map->base[idx];

    bool same_key =
        entry->key && cmp_compare(map->comparator, entry->key, key, map->key_stride) == 0;
    while (entry->key && (entry->hash != h || !same_key)) {
        idx      = idx_iter(map->capacity, idx);
        entry    = &map->base[idx];
        same_key = cmp_compare(map->comparator, entry->key, key, map->key_stride) == 0;
    }

    if (!entry->key) {
        void* new_key = pool_alloc(&map->keys, NULL);
        memcpy(new_key, key, map->key_stride);
        void* new_value = pool_alloc(&map->values, NULL);
        memcpy(new_value, value, map->value_stride);
        entry->key   = new_key;
        entry->value = new_value;
        entry->hash  = h;
    } else {
        memcpy(entry->value, value, map->value_stride);
    }
    map->size++;

    return idx;
}

i64 dict_remove(Dict* map, const void* key, void* out_value) {
    if (key == NULL || map->size == 0)
        return -1;

    Entry* e;
    i64 idx = find_entry(map, key, &e);
    if (idx == -1)
        return -1;

    if (out_value)
        memcpy(out_value, e->value, map->value_stride);

    *e = (Entry) {0};
    map->size--;
    if (!map->fixed && map->size <= map->capacity * MIN_LOAD_FACTOR)
        shrink(map);
    return idx;
}

i64 dict_get(Dict* map, const void* key, void* out_value) {
    if (key == NULL || map->size == 0)
        return -1;

    Entry* e;
    i64 idx = find_entry(map, key, &e);
    if (idx == -1)
        return -1;

    if (out_value)
        memcpy(out_value, e->value, map->value_stride);

    return idx;
}

void* dict_ref(Dict* dict, i64 idx) {
    if (dict->size == 0 || idx < 0 || idx > (i64) dict->capacity)
        return NULL;

    Entry* e = &dict->base[idx];
    if (!e->key)
        return NULL;
    return e->value;
}

void dict_foreach(const Dict* map, action action, void* data) {
    u64 j = 0;
    for (u64 i = 0; i < map->capacity && j < map->size; i++) {
        Entry* e = &map->base[i];
        if (e->key) {
            action(map, j, e->key, e->value, data);
            j++;
        }
    }
}
