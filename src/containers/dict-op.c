#include "utils/bitwise.h"
#include "dict.h"
#include "utils/hash.h"

#include <stdlib.h>
#include <string.h>

#define MAX_LOAD_FACTOR 0.75
#define MIN_LOAD_FACTOR 0.25
#define MIN_CAPACITY 4

static struct node get_node_base(const Dict* dict, u64 idx, void* base, u64 capacity) {
    struct node node = {
        .hash = 0,
        .hashp = NULL,
        .key = NULL,
        .value = NULL,
    };
    if (idx >= capacity) {
        return node;
    }

    u64 total_stride = sizeof(u64) + dict->key_stride + dict->value_stride;
    void* ptr = offset(base, idx * total_stride);
    node.hashp = ptr;
    node.hash = *node.hashp;
    node.key = offset(ptr, sizeof(u64));
    node.value = offset(ptr, sizeof(u64) + dict->key_stride);
    return node;
}

static struct node get_node(const Dict* dict, u64 idx) {
    return get_node_base(dict, idx, dict->base, dict->capacity);
}

static u64 idx_iter(u64 cap, u64 idx) {
    return (5 * idx + 1) % cap;
}

static i64 get_elem(const Dict* map, const void* key, struct node* out_node) {
    u64 h = cmp_hash(map->comparator, key, map->key_stride);
    u64 idx = h % map->capacity;
    struct node n = get_node(map, idx);
    u64 count = 0;

    while (n.hash == 0 || n.hash != h || cmp_compare(map->comparator, n.key, key, map->key_stride)) {
        if (n.hash != 0)
            count++;
        if (count == map->size)
            return FALSE;
        idx = idx_iter(map->capacity, idx);
        n = get_node(map, idx);
    }
    *out_node = n;
    return idx;
}

void dict_clear(Dict* map) {
    u64 j = map->size;
    for (u64 i = 0; i < map->capacity && j > 0; i++) {
        struct node node = get_node(map, i);
        if (node.key) {
            free(node.key);
            node.hash = 0;
            node.key = NULL;
            j--;
        }
    }
}

static void rehash_nodes(Dict* map, void* old_base, void* new_base, u64 new_capacity) {
    u64 total_stride = sizeof(u64) + map->key_stride + map->value_stride;
    for (u64 i = 0; i < map->capacity; i++) {
        struct node n = get_node_base(map, i, old_base, map->capacity);
        if (n.hash == 0)
            continue;

        u64 ni = n.hash % new_capacity;
        while (TRUE) {
            struct node new = get_node_base(map, i, new_base, new_capacity);
            if (new.hash == 0) {
                memcpy(new.hashp, n.hashp, total_stride);
                break;
            } else
                ni = idx_iter(new_capacity, ni);
        }
    }
}

static void resize(Dict* map, u64 new_capacity) {
    u64 total_stride = sizeof(u64) + map->key_stride + map->value_stride;
    void* new_base = calloc(new_capacity, total_stride);
    void* old_base = map->base;

    rehash_nodes(map, old_base, new_base, new_capacity);

    free(old_base);
    map->capacity = new_capacity;
    map->base = new_base;
}

static void grow(Dict* map) {
    resize(map, map->capacity << 1);
}

static void shrink(Dict* map) {
    if (map->capacity > MIN_CAPACITY)
        resize(map, map->capacity >> 1);
}

i64 dict_put(Dict* map, const void* key, const void* value) {
    if (key == NULL)
        return -1;

    u64 h = cmp_hash(map->comparator, key, map->key_stride);
    u64 idx = h % map->capacity;
    struct node n = get_node(map, idx);

    bool same_key = cmp_compare(map->comparator, n.key, key, map->key_stride) == 0;
    while (n.hash != 0 && (n.hash != h || !same_key)) {
        idx = idx_iter(map->capacity, idx);
        n = get_node(map, idx);
        same_key = cmp_compare(map->comparator, n.key, key, map->key_stride) == 0;
    }

    if (n.hash == 0) {
        memcpy(n.key, key, map->key_stride);
        *n.hashp = h;
    }
    memcpy(n.value, value, map->value_stride);
    map->size++;

    if (!map->fixed && map->size >= map->capacity * MAX_LOAD_FACTOR)
        grow(map);

    return idx;
}

i64 dict_remove(Dict* map, const void* key, void* outValue) {
    if (key == NULL || map->size == 0)
        return -1;

    struct node n;
    i64 idx = get_elem(map, key, &n);
    if (idx == -1)
        return -1;

    if (outValue) {
        memcpy(outValue, n.value, map->value_stride);
    }
    *n.hashp = 0;
    map->size--;
    if (!map->fixed && map->size <= map->capacity * MIN_LOAD_FACTOR)
        shrink(map);
    return idx;
}

i64 dict_get(Dict* map, const void* key, void* outValue) {
    if (key == NULL || map->size == 0)
        return -1;

    struct node n;
    i64 idx = get_elem(map, key, &n);
    if (idx == -1)
        return -1;

    if (outValue)
        memcpy(outValue, n.value, map->value_stride);

    return idx;
}

void* dict_ref(Dict* dict, i64 idx) {
    if (dict->size == 0 || idx < 0 || idx > (i64) dict->capacity)
        return NULL;

    struct node n = get_node(dict, idx);
    if (n.hash == 0)
        return NULL;
    return n.value;
}

void dict_foreach(const Dict* map, action action, void* data) {
    u64 j = 0;
    for (u64 i = 0; i < map->capacity && j < map->size; i++) {
        struct node node = get_node(map, i);
        if (node.hash != 0) {
            action(map, j, node.key, node.value, data);
            j++;
        }
    }
}
