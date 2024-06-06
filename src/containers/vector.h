#ifndef VECTOR_H
#define VECTOR_H

#include "../memory/arena.h"
#include "../definitions.h"

#include <stddef.h>

typedef struct vector
{
    Arena* arena;
    void *array;
    size_t size;
    size_t stride;
    bool external_arena;
} Vector;

void vector_init(Vector *vector, size_t initial_capacity, size_t stride);
void vector_init_with_arena(Vector *vector, Arena* arena, size_t initial_capacity, size_t stride);

void vector_empty(Vector *vector);
void vector_clear(Vector *vector);

/**
 * Free all elements stored inside the vector.
 * /!\\ The elements must be malloc'd pointers. /!\\
 */
void vector_deep_empty(Vector *vector);
void vector_deep_clear(Vector *vector);

#define vector_add_imm(vector, elem, type)                                     \
    {                                                                          \
        type holder = elem;                                                    \
        vector_add(vector, &holder);                                           \
    }

#define vector_insert_imm(vector, elem, idx, type)                             \
    {                                                                          \
        type holder = elem;                                                    \
        vector_insert(vector, &holder, idx);                                   \
    }

void vector_add(Vector *vector, void *element);
void vector_insert(Vector *vector, void *element, size_t idx);
void *vector_reserve(Vector *vector);

bool vector_remove(Vector *vector, size_t idx, void *out);
bool vector_pop(Vector *vector, void *out);
bool vector_peek(Vector *vector, void *out);

bool vector_get(Vector *vector, size_t index, void *out);
void *vector_ref(Vector *vector, size_t index);

#endif /* ! VECTOR_H */
