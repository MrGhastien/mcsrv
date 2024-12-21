#include "definitions.h"
#include "containers/vector.h"
#include "memory/arena.h"

void vector_init(Vector* vector, Arena* arena, u64 capacity, u64 stride) {
    vector->data = arena_allocate(arena, capacity * stride);
    vector->capacity = capacity;
    vector->size = 0;
    vector->stride = stride;
}

void vector_clear(Vector* vector) {
    vector->size = 0;
}
