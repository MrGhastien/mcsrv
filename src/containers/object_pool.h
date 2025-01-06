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
#include "memory/arena.h"

struct data_block;

/**
 * Structure representing a pool of objects.
 */
typedef struct ObjectPool {
    Arena* arena;             /**< The arena used to allocate new blocks to grow the pool. */
    struct data_block* start; /**< First block storing elements of this pool. */
    u32 next_insert_index;    /**< The next index of the current block at which a new element is
                                 inserted. */
    u32 capacity;             /**< The total capacity of this vector. */
    u32 size;                 /**< The number of elements stored inside this vector. */
    u32 stride;               /**< The size in bytes of elements. */
} ObjectPool;

/**
 * Initializes a new static object pool.
 *
 * @param[out] pool A pointer to an object pool structure to initialize.
 * @param[in] arena The arena to use to allocate the pool's memory blocks.
 * @param[in] capacity The maximum number of elements the pool can hold.
 * @param[in] stride The size in bytes of the elements.
 */
void objpool_init(ObjectPool* pool, Arena* arena, u32 capacity, u32 stride);
/**
 * Initializes a new dynamic object pool.
 *
 * @param[out] pool A pointer to an object pool structure to initialize.
 * @param[in] arena The arena to use to allocate the pool's memory blocks.
 * @param[in] initial_capacity The number of elements to accommodate for. The first pool's block
 * will be made large enough to hold this many elements.
 * @param[in] stride The size in bytes of the elements.
 */
void objpool_init_dynamic(ObjectPool* pool, Arena* arena, u32 initial_capacity, u32 stride);

/**
 * Removes all elements from an object pool.
 *
 * @param[inout] pool The pool to empty.
 */
void objpool_clear(ObjectPool* pool);

/**
 * Adds an element to an object pool.
 *
 * @param[inout] pool The pool to add an element into.
 * @param[out] out_index A pointer to an integer variable written with the index at which the
 * element was added in the pool.
 * @return A pointer to the object's slot inside the pool.
 */
void* objpool_add(ObjectPool* pool, i64* out_index);
/**
 * Removes an element from an object pool.
 *
 * @param[inout] pool The pool to remove an element from.
 * @param[in] idx The index of the element to remove, as returned when adding the element with @ref
 * objpool_add.
 * @return @ref TRUE if the element exists and has been removed, @ref FALSE otherwise.
 */
bool objpool_remove(ObjectPool* pool, i64 idx);

/**
 * Retrieves an element from an object pool.
 *
 * @param[in] pool The object pool to query.
 * @param[in] index The index of the element to retrieve.
 * @return A non-null pointer pointing to the element if it exists, `NULL` otherwise.
 */
void* objpool_get(ObjectPool* pool, i64 index);

/**
 * Performs an action on all the elements of an object pool.
 *
 * @param[in] pool The object pool to iterate.
 * @param[in] action The action to perform.
 * @param[in] user_data Data passed to each call of @p action.
 */
void objpool_foreach(const ObjectPool* pool, void (*action)(void*, i64, void*), void* user_data);

#define objpool_size(pool) ((pool)->size)
#define objpool_cap(pool) ((pool)->capacity)
#define objpool_stride(pool) ((pool)->stride)
#define objpool_is_dynamic(pool) ((pool)->arena != NULL)

#endif /* ! OBJECT_POOL_H */
