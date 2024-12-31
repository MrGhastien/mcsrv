#include "definitions.h"
#include <stdlib.h>
#include <string.h>

#include "containers/vector.h"
#include "logger.h"
#include "utils/bitwise.h"
#include "utils/math.h"

struct vector_block {
    u32 capacity;
    void* data;
    struct vector_block* next;
    struct vector_block* prev;
};

static struct vector_block* alloc_block(Arena* arena, u64 capacity, u64 stride) {
    struct vector_block* blk = arena_allocate(arena, sizeof *blk);
    *blk = (struct vector_block){
        .capacity = capacity,
        .data = arena_allocate(arena, stride * capacity),
    };
    return blk;
}

static struct vector_block*
get_block_from_index(const Vector* vector, u64 index, u64* out_blk_local_index) {
    struct vector_block* blk = vector->start;
    while (index >= blk->capacity) {
        index -= blk->capacity;
        blk = blk->next;
    }

    *out_blk_local_index = index;

    return blk;
}

static bool ensure_capacity(Vector* vector, u64 size) {
    if (size <= vector->capacity)
        return TRUE;

    if (!vect_is_dynamic(vector)) {
        log_error("Cannot resize a static vector !");
        return FALSE;
    }

    struct vector_block* blk = alloc_block(vector->arena, vector->capacity >> 1, vector->stride);
    blk->prev = vector->current;
    vector->current->next = blk;
    vector->capacity += blk->capacity;
    return TRUE;
}

static void register_removal(Vector* vector) {
    vector->size--;

    if (vector->next_insert_index == 0) {
        vector->current = vector->current->prev;
        vector->next_insert_index = vector->current->capacity;
    }

    vector->next_insert_index--;
}

static void
shift_elements_backwards(Vector* vector, struct vector_block* blk, u64 start, u64 global_index) {
    u64 stride = vector->stride;
    global_index += 1;
    start += 1;
    if (start == blk->capacity) {
        blk = blk->next;
        start = 0;
    }

    u64 total_to_shift = vector->size - global_index;
    while (total_to_shift > 0) {
        u64 end = clamp(start + total_to_shift, start, blk->capacity);
        void* src = offset(blk->data, start * stride);

        // Copy the first element of the block to the end of the previous block
        if (start == 0) {
            struct vector_block* prev = blk->prev;
            if (prev) {
                void* prev_dst = offset(prev->data, (prev->capacity - 1) * stride);
                memcpy(prev_dst, src, stride);
            }
            src = offset(src, stride);
            start++;
            total_to_shift--;
        }
        u64 to_shift = end - start;

        void* dst = offset(src, -stride);
        memmove(dst, src, to_shift * stride);
        total_to_shift -= to_shift;
        start = 0;
        blk = blk->next;
    }
}

static void shift_elements_forwards(Vector* vector, u64 global_index) {
    u64 stride = vector->stride;

    struct vector_block* blk = vector->current;
    u64 total_to_shift = vector->size - global_index;
    u64 end = vector->next_insert_index;
    while (total_to_shift > 0) {
        u64 start = sub_no_underflow(end, total_to_shift);
        void* src = offset(blk->data, start * stride);

        // Copy the last element of the block to the start of the next block
        if (end == blk->capacity) {
            struct vector_block* next = blk->next;
            if (next) {
                void* next_dst = next->data; // Index 0, so no offset is necessary
                memcpy(next_dst, src, stride);
            }
            src = offset(src, stride);
            total_to_shift--;
            end--;
        }
        u64 to_shift = end - start;

        void* dst = offset(src, stride);
        memmove(dst, src, to_shift * stride);

        total_to_shift -= to_shift;
        blk = blk->prev;
        end = blk->capacity;
    }
}

static void register_addition(Vector* vector) {
    vector->size++;
    vector->next_insert_index++;

    if (vector->next_insert_index == vector->current->capacity) {
        vector->current = vector->current->next;
        vector->next_insert_index = 0;
    }
}

void vect_init_dynamic(Vector* vector, Arena* arena, u64 initial_capacity, u64 stride) {
    vector->start = alloc_block(arena, initial_capacity, stride);
    vector->current = vector->start;
    vector->next_insert_index = 0;
    vector->capacity = initial_capacity;
    vector->size = 0;
    vector->stride = stride;
    vector->arena = arena;
}
void vect_init(Vector* vector, Arena* arena, u64 capacity, u64 stride) {
    vector->start = alloc_block(arena, capacity, stride);
    vector->current = vector->start;
    vector->next_insert_index = 0;
    vector->capacity = capacity;
    vector->size = 0;
    vector->stride = stride;
    vector->arena = NULL;
}

void vect_clear(Vector* vector) {
    vector->size = 0;
    vector->current = vector->start;
    vector->next_insert_index = 0;
}

void vect_add(Vector* vector, const void* element) {
    u64 stride = vector->stride;

    if (!ensure_capacity(vector, vector->size + 1))
        return;

    void* dst = offset(vector->current->data, vector->next_insert_index * stride);
    memcpy(dst, element, stride);
    register_addition(vector);
}
void vect_insert(Vector* vector, const void* element, u64 idx) {

    if (!ensure_capacity(vector, vector->size + 1))
        return;

    u64 stride = vector->stride;
    u64 local_index;
    struct vector_block* blk = get_block_from_index(vector, idx, &local_index);

    shift_elements_forwards(vector, idx);
    void* target_addr = offset(blk->data, local_index * stride);
    memcpy(target_addr, element, stride);

    register_addition(vector);
}

void* vect_reserve(Vector* vector) {
    u64 stride = vector->stride;

    if (!ensure_capacity(vector, vector->size + 1))
        return NULL;

    void* dst = offset(vector->current->data, vector->next_insert_index * stride);
    register_addition(vector);
    return dst;
}

bool vect_remove(Vector* vector, u64 idx, void* out) {
    u64 size = vector->size;
    if (idx >= size)
        return FALSE;

    u64 stride = vector->stride;

    u64 local_index;
    struct vector_block* blk = get_block_from_index(vector, idx, &local_index);

    void* address = offset(blk->data, local_index * stride);

    if (out)
        memcpy(out, address, stride);

    shift_elements_backwards(vector, blk, local_index, idx);

    register_removal(vector);

    return TRUE;
}

bool vect_pop(Vector* vector, void* out) {
    bool res = vect_peek(vector, out);
    if(res)
        register_removal(vector);
    return res;
}

bool vect_peek(Vector* vector, void* out) {
    if (vect_size(vector) == 0)
        return FALSE;
    u64 stride = vect_stride(vector);
    void* src = offset(vector->current->data, vector->next_insert_index * stride);

    if(out)
        memcpy(out, src, stride);
    return TRUE;
}

bool vect_get(Vector* vector, u64 index, void* out) {
    void* elem = vect_ref(vector, index);
    if(elem && out)
        memcpy(out, elem, vector->stride);
    return elem != NULL;
}

void* vect_ref(Vector* vector, u64 index) {
    u64 size = vector->size;
    if (index >= size)
        return NULL;

    u64 stride = vector->stride;
    u64 local_index;
    struct vector_block* blk = get_block_from_index(vector, index, &local_index);

    return offset(blk->data, local_index * stride);
}
