#include "../definitions.h"
#include <stdlib.h>
#include <string.h>

#include "../containers/vector.h"
#include "../utils/bitwise.h"

void vector_add(Vector* vector, void* element) {
    size_t stride = vector->stride;

    void* dst = arena_allocate(vector->arena, vector->stride);

    memcpy(dst, element, stride);
    vector->size++;
}

void vector_insert(Vector* vector, void* element, size_t idx) {
    size_t size = vector->size;

    size_t stride = vector->stride;

    arena_allocate(vector->arena, vector->stride);

    void* target_addr = offset(vector->array, idx * stride);
    void* next_addr = offset(vector->array, (idx + 1) * stride);
    memmove(next_addr, target_addr, stride * (size - idx));
    memcpy(target_addr, element, stride);
    vector->size++;
}

void* vector_reserve(Vector* vector) {
    void* ptr = arena_allocate(vector->arena, vector->stride);
    vector->size++;
    return ptr;
}

bool vector_remove(Vector* vector, size_t idx, void* out) {
    size_t size = vector->size;
    if (idx >= size)
        return FALSE;

    size_t stride = vector->stride;

    void* address = offset(vector->array, idx * stride);

    if (out)
        memcpy(out, address, stride);

    memmove(address, offset(address, stride), (size - idx - 1) * stride);
    arena_free(vector->arena, vector->stride);
    vector->size--;
    return TRUE;
}

bool vector_pop(Vector* vector, void* out) {
    return vector_remove(vector, vector->size - 1, out);
}

bool vector_peek(Vector* vector, void* out) {
    return vector_get(vector, vector->size - 1, out);
}

bool vector_get(Vector* vector, size_t index, void* out) {
    size_t size = vector->size;
    if (index >= size)
        return FALSE;

    size_t stride = vector->stride;

    if (out)
        memcpy(out, offset(vector->array, index * stride), stride);
    return TRUE;
}

void* vector_ref(Vector* vector, size_t index) {
    size_t size = vector->size;
    if (index >= size)
        return NULL;

    size_t stride = vector->stride;

    return offset(vector->array, index * stride);
}
