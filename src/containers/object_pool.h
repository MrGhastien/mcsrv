#ifndef OBJECT_POOL_H
#define OBJECT_POOL_H

#include "definitions.h"
#include "memory/arena.h"

struct data_block;

typedef struct ObjectPool {
    Arena* arena;                 /**< The arena used to allocate new blocks to grow the vector. */
    struct data_block* start;     /**< Array of blocks storing elements of this vector. */
    u32 next_insert_index;        /**< The next index of the current block at which a new element is
                                     inserted. */
    u32 capacity; /**< The total capacity of this vector. */
    u32 size;     /**< The number of elements stored inside this vector. */
    u32 stride;   /**< The size in bytes of elements. */
} ObjectPool;

void objpool_init(ObjectPool* pool, Arena* arena, u32 capacity, u32 stride);
void objpool_init_dynamic(ObjectPool* pool, Arena* arena, u32 initial_capacity, u32 stride);

void objpool_clear(ObjectPool* pool);

void* objpool_add(ObjectPool* pool, i64* out_index);
bool objpool_remove(ObjectPool* pool, i64 idx);
void* objpool_get(ObjectPool* pool, i64 index);

#define objpool_size(pool) ((pool)->size)
#define objpool_cap(pool) ((pool)->capacity)
#define objpool_stride(pool) ((pool)->stride)
#define objpool_is_dynamic(pool) ((pool)->arena != NULL)

#endif /* ! OBJECT_POOL_H */
