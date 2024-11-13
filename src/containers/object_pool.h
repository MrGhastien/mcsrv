#ifndef OBJECT_POOL_H
#define OBJECT_POOL_H

#include "definitions.h"

#include <memory/arena.h>

typedef struct ObjectPool {
    void* array;
    u32 capacity;
    u32 size;
    u32 stride;
    u32 next_available;
} ObjectPool;

void objpool_init(ObjectPool* pool, Arena* arena, u32 initial_capacity, u32 stride);

void objpool_clear(ObjectPool* pool);

void* objpool_add(ObjectPool* pool, i64* out_index);
bool objpool_remove(ObjectPool* pool, i64 idx);
void* objpool_get(ObjectPool* pool, i64 index);

#endif /* ! OBJECT_POOL_H */
