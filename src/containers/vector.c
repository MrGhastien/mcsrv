#include "vector.h"
#include "_array_internal.h"
#include "definitions.h"
#include "logger.h"
#include "utils/bitwise.h"
#include "utils/math.h"
#include "memory/mem_tags.h"

#include <stdlib.h>
#include <string.h>

struct data_block* alloc_block(Arena* arena, u64 capacity, u64 stride, i32 mem_tag) {
    struct data_block* blk = arena_allocate(arena, sizeof *blk, mem_tag);
    *blk = (struct data_block){
        .capacity = capacity,
        .data = arena_allocate(arena, stride * capacity, mem_tag),
    };
    return blk;
}

struct data_block* get_block_from_index(struct data_block* start, u64 index, u64* out_blk_local_index) {
    while (index >= start->capacity) {
        index -= start->capacity;
        start = start->next;
    }

    if(out_blk_local_index)
        *out_blk_local_index = index;

    return start;
}

static bool ensure_capacity(Vector* vector, u64 size) {
    if (size <= vector->capacity)
        return TRUE;

    if (!vect_is_dynamic(vector)) {
        log_error("Cannot resize a static vector !");
        return FALSE;
    }

    struct data_block* blk = alloc_block(vector->arena, vector->capacity >> 1, vector->stride, ALLOC_TAG_VECTOR);
    blk->prev = vector->current;
    vector->current->next = blk;
    vector->capacity += blk->capacity;

    if (vector->next_insert_index == vector->current->capacity) {
        vector->current = vector->current->next;
        vector->next_insert_index = 0;
    }
    return TRUE;
}

static void register_addition(Vector* vector) {
    vector->size++;
    vector->next_insert_index++;
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
shift_elements_backwards(Vector* vector, struct data_block* blk, u64 start, u64 global_index) {
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
            struct data_block* prev = blk->prev;
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

    struct data_block* blk = vector->current;
    u64 total_to_shift = vector->size - global_index;
    u64 end = vector->next_insert_index;
    while (total_to_shift > 0) {
        u64 start = sub_no_underflow(end, total_to_shift);
        void* src = offset(blk->data, start * stride);

        // Copy the last element of the block to the start of the next block
        if (end == blk->capacity) {
            struct data_block* next = blk->next;
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

void vect_init_dynamic(Vector* vector, Arena* arena, u64 initial_capacity, u64 stride) {
    vector->start = alloc_block(arena, initial_capacity, stride, ALLOC_TAG_VECTOR);
    vector->current = vector->start;
    vector->next_insert_index = 0;
    vector->capacity = initial_capacity;
    vector->size = 0;
    vector->stride = stride;
    vector->arena = arena;
}
void vect_init(Vector* vector, Arena* arena, u64 capacity, u64 stride) {
    vector->start = alloc_block(arena, capacity, stride, ALLOC_TAG_VECTOR);
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
    struct data_block* blk = get_block_from_index(vector->start, idx, &local_index);

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
    struct data_block* blk = get_block_from_index(vector->start, idx, &local_index);

    void* address = offset(blk->data, local_index * stride);

    if (out)
        memcpy(out, address, stride);

    shift_elements_backwards(vector, blk, local_index, idx);

    register_removal(vector);

    return TRUE;
}

bool vect_pop(Vector* vector, void* out) {
    bool res = vect_peek(vector, out);
    if (res)
        register_removal(vector);
    return res;
}

bool vect_peek(const Vector* vector, void* out) {
    if (vect_size(vector) == 0)
        return FALSE;
    u64 stride = vect_stride(vector);
    void* src = offset(vector->current->data, vector->next_insert_index * stride);

    if (out)
        memcpy(out, src, stride);
    return TRUE;
}

bool vect_get(const Vector* vector, u64 index, void* out) {
    void* elem = vect_ref(vector, index);
    if (elem && out)
        memcpy(out, elem, vector->stride);
    return elem != NULL;
}

void* vect_ref(const Vector* vector, u64 index) {
    u64 size = vector->size;
    if (index >= size)
        return NULL;

    u64 stride = vector->stride;
    u64 local_index;
    struct data_block* blk = get_block_from_index(vector->start, index, &local_index);

    return offset(blk->data, local_index * stride);
}
