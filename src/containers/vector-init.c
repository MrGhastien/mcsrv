#include <stdbool.h>
#include <stdlib.h>

#include "../containers/vector.h"

void vector_init(Vector *vector, size_t initial_capacity, size_t stride)
{
    vector->stride = stride;
    vector->array = malloc(initial_capacity * stride);
}

void vector_empty(Vector *vector)
{
    free(vector->array);
}

void vector_clear(Vector *vector)
{
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
