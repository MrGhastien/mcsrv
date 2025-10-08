/**
 * @file
 *
 * Functions related to object pools.
 *
 * An object pool stores elements in a similar way to vectors, except the elements are not packed;
 * their indices do not change.
 *
 * This is particularly useful to keep track of multiple objects which need to be randomly accessed
 * by an identifier (in this implementation identifiers are indices in the pool's underlying array.
 */
#ifndef OBJECT_POOL_H
#define OBJECT_POOL_H

#include "definitions.h"
#include "memory/_memory_internal.h"
#include "memory/mem_tags.h"

/**
 * Structure representing a pool of objects.
 */
typedef struct PoolAllocator {
    memory_chain mem;
    struct obj_node* head;
    struct obj_node* tail;
    bool dynamic;
    u32 capacity; /**< The total capacity of this vector. */
    u32 size;     /**< The number of elements stored inside this vector. */
    u32 stride;   /**< The size in bytes of elements. */
} PoolAllocator;

/**
 * Initializes a new static object pool.
 *
 * @param[out] pool A pointer to an object pool structure to initialize.
 * @param[in] arena The arena to use to allocate the pool's memory blocks.
 * @param[in] capacity The maximum number of elements the pool can hold.
 * @param[in] stride The size in bytes of the elements.
 */
void pool_init(
    PoolAllocator* pool, u32 capacity, u32 stride, enum MemoryChainTag tag, memory_chain prev);
/**
 * Initializes a new dynamic object pool.
 *
 * @param[out] pool A pointer to an object pool structure to initialize.
 * @param[in] arena The arena to use to allocate the pool's memory blocks.
 * @param[in] initial_capacity The number of elements to accommodate for. The first pool's block
 * will be made large enough to hold this many elements.
 * @param[in] stride The size in bytes of the elements.
 */
void pool_init_dynamic(PoolAllocator* pool,
                       u32 initial_capacity,
                       u32 stride,
                       enum MemoryChainTag tag,
                       memory_chain prev);

void pool_init_static(PoolAllocator* pool, u32 capacity, u32 stride, memory_chain chain);

/**
 * Removes all elements from an object pool.
 *
 * @param[inout] pool The pool to empty.
 */
void pool_clear(PoolAllocator* pool);

void pool_destroy(PoolAllocator* pool);

/**
 * Adds an element to an object pool.
 *
 * @param[inout] pool The pool to add an element into.
 * @param[out] out_index A pointer to an integer variable written with the index at which the
 * element was added in the pool.
 * @return A pointer to the object's slot inside the pool.
 */
void* pool_alloc(PoolAllocator* pool, i64* out_index);

bool pool_free(PoolAllocator* pool, void* ptr);

/**
 * Removes an element from an object pool.
 *
 * @param[inout] pool The pool to remove an element from.
 * @param[in] idx The index of the element to remove, as returned when adding the element with @ref
 * pool_add.
 * @return @ref TRUE if the element exists and has been removed, @ref FALSE otherwise.
 */
bool pool_free_idx(PoolAllocator* pool, i64 idx);

/**
 * Retrieves an element from an object pool.
 *
 * @param[in] pool The object pool to query.
 * @param[in] index The index of the element to retrieve.
 * @return A non-null pointer pointing to the element if it exists, `NULL` otherwise.
 */
void* pool_get(PoolAllocator* pool, i64 index);

/**
 * Performs an action on all the elements of an object pool.
 *
 * @param[in] pool The object pool to iterate.
 * @param[in] action The action to perform.
 * @param[in] user_data Data passed to each call of @p action.
 */
void pool_foreach(const PoolAllocator* pool, void (*action)(void*, i64, void*), void* user_data);

#define pool_size(pool) ((pool)->size)
#define pool_cap(pool) ((pool)->capacity)
#define pool_stride(pool) ((pool)->stride)
#define pool_is_dynamic(pool) ((pool)->dynamic)

#endif /* ! OBJECT_POOL_H */
