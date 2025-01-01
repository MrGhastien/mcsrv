/**
 * @file
 */

#ifndef VECTOR_H
#define VECTOR_H

#include "definitions.h"
#include "memory/arena.h"

struct data_block;

typedef struct vector {
    struct data_block* start;   /**< Array of blocks storing elements of this vector. */
    struct data_block* current; /**< The next block in which a new element is inserted. */
    u32 next_insert_index;        /**< The next index of the current block at which a new element is
                                     inserted. */
    u32 capacity;                 /**< The total capacity of this vector. */
    u32 size;                     /**< The number of elements stored inside this vector. */
    u32 stride;                   /**< The size in bytes of elements. */
    Arena* arena;                 /**< The arena used to allocate new blocks to grow the vector. */
} Vector;

/**
 * Initializes a new static vector.
 *
 * @param[out] vector A pointer to the vector structure to initialize.
 * @param[in] arena The arena to use to allocate the vector's buffer.
 * @param[in] capacity The capacity of the vector.
 * @param[in] stride The size (in bytes) of the elements contained inside the vector.
 */
void vect_init(Vector* vector, Arena* arena, u64 capacity, u64 stride);

/**
* Initializes a new dynamic vector.
*
* @param[out] vector A pointer to the dynamic vector structure to initialize.
* @param[in] arena The arena to use for memory allocations. This is kept by the vector.
* @param[in] initial_capacity The initial capacity of the vector.
* @param[in] stride The size in bytes of the elements stored inside the vector.
*/
void vect_init_dynamic(Vector* vector, Arena* arena, u64 initial_capacity, u64 stride);

/**
 * Removes all the elements inside the specified vector.
 *
 * Clearing a vector sets its size to zero.
 *
 * @param[inout] vector The vector to clear.
 */
void vect_clear(Vector* vector);


/**
 * Adds an element at the end of a vector.
 *
 * The element is copied inside the vector's internal array.
 *
 * @param[inout] vector The vector to add an element to.
 * @param[in] element A pointer to the element to add. There must be at least as much bytes readable
 * as the stride passed to the @ref vect_init call.
 */
void vect_add(Vector* vector, const void* element);
/**
 * Inserts an element inside a vector.
 *
 * The element is copied into the vector's internal array.
 *
 * @param[inout] vector The vector to insert an element into.
 * @param[in] element A pointer to the element to insert. There must be at least as many bytes
 * readable as the stride passed to the @ref vect_init call.
 * @param[in] idx The index in the vector at which to insert the element.
 */
void vect_insert(Vector* vector, const void* element, u64 idx);

/**
 * Reserves a slot at the end of a vector.
 *
 * This function adds a new element but does not set its slot memory.
 *
 * @param[inout] vector The vector from which to reserve a slot.
 * @return A pointer to the slot inside the vector.
 */
void* vect_reserve(Vector* vector);

/**
 * Removes the element at the specified index from a vector.
 *
 * @param[inout] vector The vector to remove an element from.
 * @param[in] idx The index inside the vector at which to remove the element.
 * @param[out] out The removed element is copied at this location. Can be `NULL`.
 * @return @ref TRUE if the element was successfully removed, @ref FALSE otherwise.
 */
bool vect_remove(Vector* vector, u64 idx, void* out);
/**
 * Removes the last element of a vector.
 *
 * The call:
 * @code{c}
 * vect_pop(vector, out);
 * @endcode
 * is equivalent to the call:
 * @code{c}
 * vect_remove(vector, vect_size(vector) - 1, out);
 * @endcode
 *
 * @param[inout] vector The vector to remove an element from.
 * @paran[out] out The removed element is copied at this location. Can be `NULL`.
 * @return @ref TRUE if the element was successfully removed, @ref FALSE otherwise.
 */
bool vect_pop(Vector* vector, void* out);

/**
 * Retrieves the last element of a vector.
 *
 * This is equivalent to @ref vector_pop, but the element is not actually removed.
 *
 * @param[inout] vector The vector to query.
 * @param[out] out A pointer to a memory region where the queried element will be copied.
 * @return @ref TRUE if the last element was successfully retrieved, @ref FALSE otherwise.
 */
bool vect_peek(const Vector* vector, void* out);
/**
 * Retrieves an element of a vector.
 *
 * @param[inout] vector The vector to query.
 * @param[in] index The index inside a vector of the element to retrieve.
 * @param[out] out A pointer to a memory region where the queried element will be copied.
 * @return @ref TRUE if the element was successfully retrieved, @ref FALSE otherwise.
 */
bool vect_get(const Vector* vector, u64 index, void* out);
/**
 * Returns a reference to an element inside a vector.
 *
 * This is a convenience function to avoid copying elements around. Be careful not to overwrite
 * other elements, as no mechanism ensures memory safety inside a vector !
 *
 * @param[inout] vector The vector to query.
 * @param[in] index The index inside the vector of the element to query.
 * @return A pointer to the slot in which the queried element is stored, `NULL` if the element could
 * not be retrieved.
 */
void* vect_ref(const Vector* vector, u64 index);

/**
 * Convenience macro to add an immediate value at the end of a vector.
 *
 * @param[inout] vector The vector to which to add the value.
 * @param[in] elem The value to add into the vector.
 * @param[in] type The type of the element to add.
 */
#define vect_add_imm(vector, elem, type)                                                           \
    {                                                                                              \
        typeof(elem) holder = elem;                                                                \
        vect_add(vector, &holder);                                                                 \
    }

/**
 * Convenience macro to insert an immediate value into a vector.
 *
 * @param[inout] vector The vector to which to add the value.
 * @param[in] elem The value to add into the vector.
 * @param[in] idx The index in the vector at which to add the element.
 * @param[in] type The type of the element to add.
 */
#define vect_insert_imm(vector, elem, idx, type)                                                   \
    {                                                                                              \
        typeof(elem) holder = elem;                                                                \
        vect_insert(vector, &holder, idx);                                                         \
    }

#define vect_size(vector) ((vector)->size)
#define vect_cap(vector) ((vector)->capacity)
#define vect_stride(vector) ((vector)->stride)
#define vect_is_dynamic(vector) ((vector)->arena != NULL)


#endif /* ! VECTOR_H */
