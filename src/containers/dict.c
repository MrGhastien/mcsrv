#include "dict.h"
#include "../utils/bitwise.h"

#include <stdlib.h>
#include <string.h>

#define MAX_LOAD_FACTOR 0.75
#define MIN_LOAD_FACTOR 0.25
#define MIN_CAPACITY 4
#define DEFAULT_CAP 4

// Hash function used in sdbm
static unsigned long hash(const char* str) {
    unsigned long hash = 0;
    int c;

    while ((c = *(str++)))
        hash = c + (hash << 6) + (hash << 16) - hash;

    return hash;
}

void dict_init(Dict* map, size_t key_stride, size_t value_stride) {
    map->capacity = DEFAULT_CAP;
    map->size = 0;
    map->arena = malloc(sizeof(Arena));
    if (!map->arena)
        return;
    *map->arena = arena_create();
    size_t total_stride = sizeof(u64) + key_stride + value_stride;
    map->base = arena_allocate(map->arena, total_stride * map->capacity);
    map->key_stride = key_stride;
    map->value_stride = value_stride;
}

static void free_elem(const Dict* dict, void* key, void* value, void* data) {
    (void)dict;
    (void)value;
    (void)data;
    free(key);
}

void dict_destroy(Dict* map) {
    dict_foreach(map, &free_elem, NULL);
    arena_destroy(map->arena);
    if (map->external_arena)
        free(map->arena);
}

static struct node get_node_base(Dict* dict, size_t idx, void* base, size_t capacity) {
    struct node node;
    size_t total_stride = sizeof(u64) + dict->key_stride + dict->value_stride;

    void* ptr = offset(base, idx * total_stride);
    node.hashp = ptr;
    node.hash = *node.hashp;
    node.key = offset(ptr, sizeof(u64));
    node.value = offset(ptr, sizeof(u64) + dict->key_stride);
    return node;
}

static struct node get_node(Dict* dict, size_t idx) {
    return get_node_base(dict, idx, dict->base, dict->capacity);
}

void dict_clear(Dict* map) {
    size_t j = map->size;
    for (size_t i = 0; i < map->capacity && j > 0; i++) {
        struct node node = get_node(map, i);
        if (node.key) {
            free(node.key);
            node.hash = 0;
            node.key = NULL;
            j--;
        }
    }
}

static size_t idx_iter(size_t cap, size_t idx) {
    return (5 * idx + 1) % cap;
}

static void rehash_nodes(Dict* map, void* old_base, void* new_base, size_t new_capacity) {
    for (size_t i = 0; i < map->capacity; i++) {
        struct node n = get_node_base(map, i, old_base, map->capacity);
        if (n.hash == 0)
            continue;

        size_t ni = n.hash % new_capacity;
        while (TRUE) {
            struct node new = get_node_base(map, i, new_base, new_capacity);
            if (new.hash == 0) {
                *new.hashp = n.hash;
                memcpy(new.key, n.key, map->key_stride + map->value_stride);
                break;
            } else
                ni = idx_iter(new_capacity, ni);
        }
    }
}

static void resize(Dict* map, size_t new_capacity) {
    size_t total_stride = sizeof(u64) + map->key_stride + map->value_stride;
    void* old_base;
    void* new_base;

    if (new_capacity > map->capacity) {
        arena_allocate(map->arena, new_capacity - map->capacity);
        new_base = map->base;
        old_base = arena_allocate(map->arena, map->capacity);
        memmove(old_base, map->base, total_stride * map->capacity);
    } else {
        new_base = arena_allocate(map->arena, new_capacity);
        old_base = map->base;
    }

    rehash_nodes(map, old_base, new_base, new_capacity);

    if (new_capacity < map->capacity)
        memmove(map->base, new_base, total_stride * new_capacity);

    arena_free(map->arena, map->capacity);
    map->capacity = new_capacity;
}

static void grow(Dict* map) {
    resize(map, map->capacity << 1);
}

static void shrink(Dict* map) {
    if (map->capacity > MIN_CAPACITY)
        resize(map, map->capacity >> 1);
}

void dict_put(Dict* map, void* key, void* value) {
    if (key == NULL)
        return;

    u64 h = hash(key);
    size_t idx = h % map->capacity;
    struct node n = get_node(map, idx);

    while (n.hash != 0 && n.hash != h && memcmp(n.key, key, map->key_stride)) {
        idx = idx_iter(map->capacity, idx);
        n = get_node(map, idx);
    }

    if (n.hash == 0) {
        memcpy(n.key, key, map->key_stride);
        *n.hashp = h;
    }
    memcpy(n.value, value, map->value_stride);
    map->size++;

    if (map->size >= map->capacity * MAX_LOAD_FACTOR)
        grow(map);
}

bool dict_remove(Dict* map, void* key, void* outValue) {
    if (key == NULL || map->size == 0)
        return FALSE;

    unsigned long h = hash(key);
    size_t idx = h % map->capacity;
    struct node n = get_node(map, idx);
    size_t count = 0;

    while (n.hash == 0 || n.hash != h || memcmp(n.key, key, map->key_stride)) {
        if (n.hash != 0)
            count++;
        if (count == map->size)
            return FALSE;
        idx = idx_iter(map->capacity, idx);
        n = get_node(map, idx);
    }

    if (outValue) {
        memcpy(outValue, n.value, map->value_stride);
    }
    *n.hashp = 0;
    map->size--;
    if (map->size <= map->capacity * MIN_LOAD_FACTOR)
        shrink(map);
    return TRUE;
}

bool dict_get(Dict* map, void* key, void* outValue) {
    if (key == NULL || map->size == 0)
        return FALSE;

    unsigned long h = hash(key);
    size_t idx = h % map->capacity;
    struct node n = get_node(map, idx);
    size_t c = 0;

    while (n.hash == 0 || n.hash != h || memcmp(n.key, key, map->key_stride)) {
        if (n.hash == 0)
            c++;
        if (c == map->size)
            return FALSE;
        idx = idx_iter(map->capacity, idx);
        n = get_node(map, idx);
    }

    if (outValue)
        memcpy(outValue, n.value, map->value_stride);

    return TRUE;
}

void dict_foreach(Dict* map, action action, void* data) {
    size_t j = map->size;
    for (size_t i = 0; i < map->capacity && j > 0; i++) {
        struct node node = get_node(map, i);
        if (node.hash != 0) {
            action(map, node.key, node.value, data);
            j--;
        }
    }
}

static void deep_free(const Dict* dict, void* key, void* value, void* data) {
    (void)dict;
    (void)value;
    (void)data;
    void** p = value;
    free(*p);
}

void dict_deep_destroy(Dict* map) {
    dict_foreach(map, &deep_free, NULL);
    dict_destroy(map);
}
