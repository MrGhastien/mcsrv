#include "object_pool.h"
#include "_array_internal.h"
#include "logger.h"

#include <stdlib.h>
#include <utils/bitwise.h>

struct PoolObj {
    bool is_allocated;
    void* object;
};

void objpool_init(ObjectPool* pool, Arena* arena, u32 capacity, u32 stride) {
    pool->start = alloc_block(arena, capacity, stride);
    pool->next_insert_index = 0;
    pool->capacity = capacity;
    pool->size = 0;
    pool->stride = stride;
    pool->arena = NULL;
}

void objpool_init_dynamic(ObjectPool* pool, Arena* arena, u32 initial_capacity, u32 stride) {
    pool->start = alloc_block(arena, initial_capacity, stride);
    pool->next_insert_index = 0;
    pool->capacity = initial_capacity;
    pool->size = 0;
    pool->stride = stride;
    pool->arena = NULL;
}

static void set_allocated(const struct data_block* blk, u64 index, bool allocated, u64 stride) {
    bool* ptr = offset(blk->data, index * (stride + sizeof(bool)));
    *ptr = allocated;
}

static bool is_allocated(const struct data_block* blk, u64 index, u64 stride) {
    const bool* ptr = offset(blk->data, index * (stride + sizeof(bool)));
    return *ptr;
}

static void* get_obj(const struct data_block* blk, u64 index, u64 stride) {
    return offset(blk->data, index * (stride + sizeof(bool)) + sizeof(bool));
}

static void update_next_available(ObjectPool* pool) {
    u64 global_index = pool->next_insert_index;
    u64 i;
    struct data_block* blk = get_block_from_index(pool->start, pool->next_insert_index, &i);
    while (blk) {
        while (i < blk->capacity) {
            if (!is_allocated(blk, i, pool->stride)) {
                pool->next_insert_index = global_index;
                return;
            }
            i++;
            global_index++;
        }
        blk = blk->next;
    }
}

static bool ensure_capacity(ObjectPool* pool, u64 size) {
    if (size <= pool->capacity)
        return TRUE;

    if (!objpool_is_dynamic(pool)) {
        log_error("Cannot resize a static vector !");
        return FALSE;
    }

    struct data_block* blk = alloc_block(pool->arena, pool->capacity >> 1, pool->stride);
    struct data_block* last = pool->start;
    while (last->next)
        last = last->next;
    blk->prev = last;
    last->next = blk;
    pool->capacity += blk->capacity;
    return TRUE;
}

void objpool_clear(ObjectPool* pool) {
    for (struct data_block* blk = pool->start; blk; blk = blk->next) {
        for (u64 i = 0; i < blk->capacity; i++) {
            set_allocated(blk, i, FALSE, pool->stride);
        }
    }
    pool->size = 0;
    pool->next_insert_index = 0;
}

void* objpool_add(ObjectPool* pool, i64* out_index) {
    u32 index = pool->next_insert_index;
    if (!ensure_capacity(pool, index + 1))
        return NULL;

    u64 local_index;
    struct data_block* blk = get_block_from_index(pool->start, index, &local_index);
    void* ptr = get_obj(blk, local_index, pool->stride);
    set_allocated(blk, local_index, TRUE, pool->stride);

    if (out_index)
        *out_index = index;
    update_next_available(pool);
    pool->size++;
    return ptr;
}

bool objpool_remove(ObjectPool* pool, i64 idx) {
    if (idx < 0 || idx >= pool->capacity)
        return FALSE;

    u64 local_index;
    struct data_block* blk = get_block_from_index(pool->start, idx, &local_index);

    if (!is_allocated(blk, local_index, pool->stride))
        return FALSE;

    set_allocated(blk, local_index, FALSE, pool->stride);
    if (idx < pool->next_insert_index)
        pool->next_insert_index = idx;
    pool->size--;
    return TRUE;
}

void* objpool_get(ObjectPool* pool, i64 index) {
    if (index < 0 || index >= pool->capacity)
        return NULL;

    u64 local_index;
    struct data_block* blk = get_block_from_index(pool->start, index, &local_index);

    if (!is_allocated(blk, local_index, pool->stride))
        return NULL;
    return get_obj(blk, local_index, pool->stride);
}

void objpool_foreach(const ObjectPool* pool, void (*action)(void*, i64)) {
    u64 size = objpool_size(pool);
    u64 cap = objpool_cap(pool);
    u64 i = 0;
    for (u64 j = 0; j < cap && i < size; ++j) {
        u64 local_index;
        struct data_block* blk = get_block_from_index(pool->start, j, &local_index);
        if (is_allocated(blk, local_index, pool->stride)) {
            action(get_obj(blk, local_index, pool->stride), j);
            i++;
        }
    }
}
