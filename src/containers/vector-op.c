#include "../definitions.h"
#include <stdlib.h>
#include <string.h>

#include "../containers/vector.h"
#include "../utils/bitwise.h"

static bool ensure_capacity(Vector* vector, size_t size) {
    if (vector->fixed || size <= vector->capacity)
        return TRUE;

    size_t new_cap = ceil_two_pow(size);
    void* new_array = realloc(vector->array, new_cap * vector->stride);
    if (!new_array)
        return FALSE;

    vector->array = new_array;
    vector->capacity = new_cap;
    return TRUE;
}

void vector_add(Vector* vector, const void* element) {
    size_t stride = vector->stride;

    if (!ensure_capacity(vector, vector->size + 1))
        return;

    memcpy(offset(vector->array, vector->size * stride), element, stride);
    vector->size++;
}

void vector_insert(Vector* vector, const void* element, size_t idx) {
    size_t size = vector->size;
    size_t stride = vector->stride;

    if (!ensure_capacity(vector, size + 1))
        return;

    void* target_addr = offset(vector->array, idx * stride);
    void* next_addr = offset(vector->array, (idx + 1) * stride);
    memmove(next_addr, target_addr, stride * (size - idx));
    memcpy(target_addr, element, stride);
    vector->size++;
}

void* vector_reserve(Vector* vector) {
    if (!ensure_capacity(vector, vector->size + 1))
        return NULL;
    void* ptr = offset(vector->array, vector->size * vector->stride);
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

    vector->size--;
    return TRUE;
}

bool vector_pop(Vector* vector, void* out) {
    return vector_remove(vector, vector->size - 1, out);
}

bool vector_peek(const Vector* vector, void* out) {
    return vector_get(vector, vector->size - 1, out);
}

bool vector_get(const Vector* vector, size_t index, void* out) {
    size_t size = vector->size;
    if (index >= size)
        return FALSE;

    size_t stride = vector->stride;

    if (out)
        memcpy(out, offset(vector->array, index * stride), stride);
    return TRUE;
}

void* vector_ref(const Vector* vector, size_t index) {
    size_t size = vector->size;
    if (index >= size)
        return NULL;

    size_t stride = vector->stride;

    return offset(vector->array, index * stride);
}
