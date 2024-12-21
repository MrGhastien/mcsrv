#include "definitions.h"
#include <stdlib.h>
#include <string.h>

#include "containers/vector.h"
#include "logger.h"
#include "utils/bitwise.h"

static bool ensure_capacity(Vector* vector, u64 size) {
    if (size <= vector->capacity)
        return TRUE;

    log_error("Cannot resize a fixed vector !");
    abort();
    return FALSE;
}

void vect_add(Vector* vector, void* element) {
    u64 stride = vector->stride;

    if (!ensure_capacity(vector, vector->size + 1))
        return;

    memcpy(offset(vector->data, vector->size * stride), element, stride);
    vector->size++;
}

void vect_insert(Vector* vector, void* element, u64 idx) {
    u64 size = vector->size;
    u64 stride = vector->stride;

    if (!ensure_capacity(vector, size + 1))
        return;

    void* target_addr = offset(vector->data, idx * stride);
    void* next_addr = offset(vector->data, (idx + 1) * stride);
    memmove(next_addr, target_addr, stride * (size - idx));
    memcpy(target_addr, element, stride);
    vector->size++;
}

void* vect_reserve(Vector* vector) {
    if (!ensure_capacity(vector, vector->size + 1))
        return NULL;
    void* ptr = offset(vector->data, vector->size * vector->stride);
    vector->size++;
    return ptr;
}

bool vect_remove(Vector* vector, u64 idx, void* out) {
    u64 size = vector->size;
    if (idx >= size)
        return FALSE;

    u64 stride = vector->stride;

    void* address = offset(vector->data, idx * stride);

    if (out)
        memcpy(out, address, stride);

    memmove(address, offset(address, stride), (size - idx - 1) * stride);

    vector->size--;
    return TRUE;
}

bool vect_pop(Vector* vector, void* out) {
    return vect_remove(vector, vector->size - 1, out);
}

bool vect_peek(Vector* vector, void* out) {
    return vect_get(vector, vector->size - 1, out);
}

bool vect_get(Vector* vector, u64 index, void* out) {
    u64 size = vector->size;
    if (index >= size)
        return FALSE;

    u64 stride = vector->stride;

    if (out)
        memcpy(out, offset(vector->data, index * stride), stride);
    return TRUE;
}

void* vect_ref(Vector* vector, u64 index) {
    u64 size = vector->size;
    if (index >= size)
        return NULL;

    u64 stride = vector->stride;

    return offset(vector->data, index * stride);
}
