#include <stdlib.h>

#include "../containers/vector.h"
#include "memory/arena.h"

void vector_init(Vector* vector, size_t initial_capacity, size_t stride) {
    vector->array = malloc(stride * initial_capacity);
    vector->capacity = initial_capacity;
    vector->size = 0;
    vector->stride = stride;
    vector->fixed = FALSE;
}

void vector_init_fixed(Vector* vector, Arena* arena, size_t capacity, size_t stride) {
    vector->array = arena_allocate(arena, capacity * stride);
    vector->capacity = capacity;
    vector->size = 0;
    vector->stride = stride;
    vector->fixed = TRUE;
}

void vector_destroy(Vector* vector) {
    if (!vector->fixed)
        free(vector->array);
}

void vector_clear(Vector* vector) {
    vector->size = 0;
}

static void vector_deep_free(Vector* vector) {
    if (vector->stride == sizeof(void*)) {
        for (size_t i = 0; i < vector->size; i++) {
            void** p = vector_ref(vector, i);
            free(*p);
        }
    }
}

void vector_deep_destroy(Vector* vector) {
    vector_deep_free(vector);
    vector_destroy(vector);
}

void vector_deep_clear(Vector* vector) {
    vector_deep_free(vector);
    vector_clear(vector);
}
