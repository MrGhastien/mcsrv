#include "../definitions.h"
#include <stdlib.h>
#include <string.h>

#include "../containers/vector.h"
#include "../utils/bitwise.h"

static bool ensure_capacity(Vector* vector, size_t capacity) {
    if (vector->capacity >= capacity)
        return TRUE;

    size_t new_capacity = vector->capacity;
    while (new_capacity < capacity)
        new_capacity <<= 1;

    void* new_array = realloc(vector->array, new_capacity * vector->stride);
    if (!new_array)
        return FALSE;

    vector->array = new_array;
    vector->capacity = new_capacity;
    return TRUE;
}

void vector_add(Vector* vector, void* element) {
    size_t size = vector->size;

    if (!ensure_capacity(vector, size + 1))
        return;

    size_t stride = vector->stride;

    memcpy(offset(vector->array, size * stride), element, stride);
    vector->size++;
}

void vector_insert(Vector* vector, void* element, size_t idx) {
    size_t size = vector->size;

    if (!ensure_capacity(vector, size + 1))
        return;

    size_t stride = vector->stride;

    void* target_addr = offset(vector->array, idx * stride);
    void* next_addr = offset(vector->array, (idx + 1) * stride);
    memmove(next_addr, target_addr, stride * (size - idx));
    memcpy(target_addr, element, stride);
    vector->size++;
}

void* vector_reserve(Vector* vector) {
    size_t size = vector->size;
    if (!ensure_capacity(vector, size + 1))
        return NULL;

    vector->size++;
    return vector_ref(vector, vector->size - 1);
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
