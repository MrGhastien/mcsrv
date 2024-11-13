#include "object_pool.h"

#include <stdint.h>
#include <stdlib.h>
#include <utils/bitwise.h>

struct PoolObj {
    bool is_allocated;
    void* object;
};

void objpool_init(ObjectPool* pool, Arena* arena, u32 initial_capacity, u32 stride) {
    *pool = (ObjectPool) {
        .array = arena_callocate(arena, initial_capacity * (stride + sizeof(bool))),
        .capacity = initial_capacity,
        .size = 0,
        .stride = stride,
        .next_available = 0,
    };

    if(!pool->array)
        abort();
}

static void set_allocated(ObjectPool* pool, i64 index, bool allocated) {
    bool* ptr = offset(pool->array, index * (pool->stride + sizeof(bool)));
    *ptr = allocated;
}

static bool is_allocated(const ObjectPool* pool, i64 index) {
    bool* ptr = offset(pool->array, index * (pool->stride + sizeof(bool)));
    return *ptr;
}

static void* get_obj(const ObjectPool* pool, i64 index) {
    return offset(pool->array, index * (pool->stride + sizeof(bool)) + sizeof(bool));
}

static void update_next_available(ObjectPool* pool) {
    u32 i = pool->next_available;
    while(i < pool->capacity && is_allocated(pool, i)) {
        i++;
    }
    pool->next_available = i;
}

void objpool_clear(ObjectPool* pool) {
    for(i64 i = 0; i < pool->capacity; i++) {
        set_allocated(pool, i, FALSE);
    }
    pool->size = 0;
    pool->next_available = 0;
}

void* objpool_add(ObjectPool* pool, i64* out_index) {
    i64 index = pool->next_available;
    void* ptr = get_obj(pool, index);
    set_allocated(pool, index, TRUE);
    if(out_index)
        *out_index = index;
    update_next_available(pool);
    return ptr;
}

bool objpool_remove(ObjectPool* pool, i64 idx) {
    if(idx < 0 || idx >= pool->capacity)
        return FALSE;

    if(!is_allocated(pool, idx))
        return FALSE;

    set_allocated(pool, idx, FALSE);
    pool->next_available = idx;
    return TRUE;
}

void* objpool_get(ObjectPool* pool, i64 index) {
    if(index < 0 || index >= pool->capacity)
        return NULL;

    if(!is_allocated(pool, index))
        return NULL;
    return get_obj(pool, index);
}
