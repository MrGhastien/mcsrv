#include <stdlib.h>

#include "../containers/vector.h"

void vector_init(Vector *vector, size_t initial_capacity, size_t stride)
{
    Arena* arena = malloc(sizeof(Arena));
    *arena = arena_create();
    vector_init_with_arena(vector, arena, initial_capacity, stride);
}

void vector_init_with_arena(Vector *vector, Arena *arena, size_t initial_capacity, size_t stride) {
    vector->stride = stride;
    vector->arena = arena;
    arena_reserve(arena, initial_capacity * stride);
    vector->array = arena_allocate(vector->arena, 0);
    vector->size = 0;
}

void vector_empty(Vector *vector)
{
    arena_free(vector->arena, vector->size * vector->stride);
    if(!vector->external_arena)
        free(vector->arena);
}

void vector_clear(Vector *vector)
{
    arena_free(vector->arena, vector->size * vector->stride);
    vector->size = 0;
}

static void vector_deep_free(Vector *vector)
{
    if (vector->stride == sizeof(void *))
    {
        for (size_t i = 0; i < vector->size; i++)
        {
            void **p = vector_ref(vector, i);
            free(*p);
        }
    }
}

void vector_deep_empty(Vector *vector)
{
    vector_deep_free(vector);
    vector_empty(vector);
}

void vector_deep_clear(Vector *vector)
{
    vector_deep_free(vector);
    vector_clear(vector);
}
