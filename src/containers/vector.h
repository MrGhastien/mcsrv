#ifndef VECTOR_H
#define VECTOR_H

#include "../memory/arena.h"
#include "../definitions.h"

#include <stddef.h>

typedef struct vector
{
    void *array;
    u32 capacity;
    u32 size;
    u32 stride;
    bool fixed;
} Vector;

void vector_init(Vector *vector, size_t initial_capacity, size_t stride);
void vector_init_fixed(Vector *vector, Arena* arena, size_t capacity, size_t stride);

void vector_destroy(Vector *vector);
void vector_clear(Vector *vector);

/**
 * Free all elements stored inside the vector.
 * /!\\ The elements must be malloc'd pointers. /!\\
 */
void vector_deep_destroy(Vector *vector);
void vector_deep_clear(Vector *vector);

#define vector_add_imm(vector, elem, type)                                     \
    {                                                                          \
        typeof(elem) holder = elem;                                                    \
        vector_add(vector, &holder);                                           \
    }

#define vector_insert_imm(vector, elem, idx, type)                             \
    {                                                                          \
        type holder = elem;                                                    \
        vector_insert(vector, &holder, idx);                                   \
    }

void vector_add(Vector *vector, const void *element);
void vector_insert(Vector *vector, const void *element, size_t idx);
void *vector_reserve(Vector *vector);

bool vector_remove(Vector *vector, size_t idx, void *out);
bool vector_pop(Vector *vector, void *out);
bool vector_peek(const Vector* vector, void* out);

bool vector_get(const Vector* vector, size_t index, void* out);
void *vector_ref(const Vector* vector, size_t index);

#endif /* ! VECTOR_H */
