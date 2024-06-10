#include "../utils/bitwise.h"
#include "dict.h"

#include <stdlib.h>
#include <string.h>

#define MAX_LOAD_FACTOR 0.75
#define MIN_LOAD_FACTOR 0.25
#define MIN_CAPACITY 4

// Hash function used in sdbm
static u64 hash(const u8* data, size_t size) {
    u64 hash = 37;
    u8 b;

    for(size_t i = 0; i < size; i++) {
        b = data[i];
        hash = b + (hash << 6) + (hash << 16) - hash;
    }

    return hash;
}

static struct node get_node_base(Dict* dict, size_t idx, void* base, size_t capacity) {
    struct node node;
    if (idx >= capacity) {
        node.hash = 0;
        node.hashp = 0;
        node.key = 0;
        node.value = 0;
        return node;
    }

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
    size_t total_stride = sizeof(u64) + map->key_stride + map->value_stride;
    for (size_t i = 0; i < map->capacity; i++) {
        struct node n = get_node_base(map, i, old_base, map->capacity);
        if (n.hash == 0)
            continue;

        size_t ni = n.hash % new_capacity;
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

static void resize(Dict* map, size_t new_capacity) {
    size_t total_stride = sizeof(u64) + map->key_stride + map->value_stride;
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

void dict_put(Dict* map, const void* key, const void* value) {
    if (key == NULL)
        return;

    u64 h = hash(key, map->key_stride);
    size_t idx = h % map->capacity;
    struct node n = get_node(map, idx);

    /*
      hash == 0 => libre
      hash == h && same_key => libre
      hash != h => occupé
      hash == h && !same_key => occupé

      O X X => 0
      1 0 0 => 0
      1 0 1 => 1 (hash == h && clé diff)
      1 1 0 => erreur (hash != h && clé égale) hash(x) != hash(y) => x != y
      1 1 1 => 1 (hash != h && clé diff)
     */
    bool same_key = memcmp(n.key, key, map->key_stride) == 0;
    while (n.hash != 0 && (n.hash != h || !same_key)) {
        idx = idx_iter(map->capacity, idx);
        n = get_node(map, idx);
        same_key = memcmp(n.key, key, map->key_stride) == 0;
    }

    if (n.hash == 0) {
        memcpy(n.key, key, map->key_stride);
        *n.hashp = h;
    }
    memcpy(n.value, value, map->value_stride);
    map->size++;

    if (!map->fixed && map->size >= map->capacity * MAX_LOAD_FACTOR)
        grow(map);
}

bool dict_remove(Dict* map, void* key, void* outValue) {
    if (key == NULL || map->size == 0)
        return FALSE;

    u64 h = hash(key, map->key_stride);
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
    if (!map->fixed && map->size <= map->capacity * MIN_LOAD_FACTOR)
        shrink(map);
    return TRUE;
}

bool dict_get(Dict* map, void* key, void* outValue) {
    if (key == NULL || map->size == 0)
        return FALSE;

    u64 h = hash(key, map->key_stride);
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
    size_t j = 0;
    for (size_t i = 0; i < map->capacity && j < map->size; i++) {
        struct node node = get_node(map, i);
        if (node.hash != 0) {
            action(map, j, node.key, node.value, data);
            j++;
        }
    }
}
