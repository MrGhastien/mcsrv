#ifndef VECTOR_H
#define VECTOR_H

#include "definitions.h"
#include "memory/arena.h"

#include <stddef.h>

struct vector_block;

typedef struct dvector {
    struct vector_block* start;   /**< Array of blocks storing elements of this vector. */
    struct vector_block* current; /**< The next block in which a new element is inserted. */
    u32 next_insert_index;        /**< The next index of the current block at which a new element is
                                     inserted. */
    u32 capacity;                 /**< The total capacity of this vector. */
    u32 size;                     /**< The number of elements stored inside this vector. */
    u32 stride;                   /**< The size in bytes of elements. */
    Arena* arena;                 /**< The arena used to allocate new blocks to grow the vector. */
} DynVector;

typedef struct vector {
    void* data;
    u32 capacity;
    u32 size;
    u32 stride;
} Vector;

void vect_init(Vector* vector, Arena* arena, u64 capacity, u64 stride);

void vect_clear(Vector* vector);

#define vect_add_imm(vector, elem, type)                                                         \
    {                                                                                              \
        typeof(elem) holder = elem;                                                                \
        vect_add(vector, &holder);                                                               \
    }

#define vect_insert_imm(vector, elem, idx, type)                                                 \
    {                                                                                              \
        typeof(elem) holder = elem;                                                                \
        vect_insert(vector, &holder, idx);                                                       \
    }

void vect_add(Vector* vector, void* element);
void vect_insert(Vector* vector, void* element, u64 idx);
void* vect_reserve(Vector* vector);

bool vect_remove(Vector* vector, u64 idx, void* out);
bool vect_pop(Vector* vector, void* out);
bool vect_peek(Vector* vector, void* out);

bool vect_get(Vector* vector, u64 index, void* out);
void* vect_ref(Vector* vector, u64 index);

void dvect_init(DynVector* vector, Arena* arena, u64 initial_capacity, u64 stride);

void dvect_clear(DynVector* vector);

#define dvect_add_imm(vector, elem, type)                                                         \
    {                                                                                              \
        typeof(elem) holder = elem;                                                                \
        dvect_add(vector, &holder);                                                               \
    }

#define dvect_insert_imm(vector, elem, idx, type)                                                 \
    {                                                                                              \
        typeof(elem) holder = elem;                                                                \
        dvect_insert(vector, &holder, idx);                                                       \
    }

void dvect_add(DynVector* vector, void* element);
void dvect_insert(DynVector* vector, void* element, u64 idx);
void* dvect_reserve(DynVector* vector);

bool dvect_remove(DynVector* vector, u64 idx, void* out);
bool dvect_pop(DynVector* vector, void* out);
bool dvect_peek(DynVector* vector, void* out);

bool dvect_get(DynVector* vector, u64 index, void* out);
void* dvect_ref(DynVector* vector, u64 index);

#endif /* ! VECTOR_H */
