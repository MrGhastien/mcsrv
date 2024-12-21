#include "containers/vector.h"
#include "definitions.h"
#include "utils/bitwise.h"
#include "utils/math.h"
#include <string.h>

struct vector_block {
    u32 capacity;
    void* data;
    struct vector_block* next;
    struct vector_block* prev;
};

void dvect_init(DynVector* vector, Arena* arena, u64 initial_capacity, u64 stride) {
    vector->start = arena_allocate(arena, sizeof(struct vector_block));
    *vector->start = (struct vector_block){
        .capacity = initial_capacity,
        .data = arena_allocate(arena, stride * initial_capacity),
        .next = NULL,
        .prev = NULL,
    };
    vector->current = vector->start;
    vector->next_insert_index = 0;
    vector->capacity = initial_capacity;
    vector->size = 0;
    vector->stride = stride;
    vector->arena = arena;
}

static bool ensure_capacity(DynVector* vector, u64 size) {
    if (size <= vector->capacity)
        return TRUE;

    struct vector_block* blk = arena_allocate(vector->arena, sizeof *blk);
    blk->capacity = vector->capacity >> 1;
    blk->data = arena_allocate(vector->arena, blk->capacity * vector->stride);
    blk->prev = vector->current;
    vector->current->next = blk;

    vector->capacity += blk->capacity;

    return TRUE;
}

static void
shift_elements_backwards(DynVector* vector, struct vector_block* blk, u64 start, u64 global_index) {
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

static void shift_elements_forwards(DynVector* vector, u64 global_index) {
    u64 stride = vector->stride;
    global_index += 1;

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
        blk = blk->next;
        end = blk->capacity;
    }
}

static struct vector_block*
get_block_from_index(const DynVector* vector, u64 index, u64* out_blk_local_index) {
    struct vector_block* blk = vector->start;
    while (index >= blk->capacity) {
        index -= blk->capacity;
        blk = blk->next;
    }

    *out_blk_local_index = index;

    return blk;
}

static void register_removal(DynVector* vector) {
    vector->size--;

    if (vector->next_insert_index == 0) {
        vector->current = vector->current->prev;
        vector->next_insert_index = vector->current->capacity;
    } else
        vector->next_insert_index--;
}

static void register_addition(DynVector* vector) {
    vector->size++;
    vector->next_insert_index++;

    if (vector->next_insert_index == vector->current->capacity) {
        ensure_capacity(vector, vector->size + 1);
        vector->current = vector->current->next;
        vector->next_insert_index = 0;
    }
}

void dvect_add(DynVector* vector, void* element) {
    u64 stride = vector->stride;

    // No need to ensure there is enough capacity here,
    // because it is ensured when registering the previous addition
    // for the next one

    void* dst = offset(vector->current->data, vector->next_insert_index * stride);
    memcpy(dst, element, stride);

    register_addition(vector);
}

void dvect_insert(DynVector* vector, void* element, u64 idx) {
    u64 stride = vector->stride;

    u64 local_index;
    struct vector_block* blk = get_block_from_index(vector, idx, &local_index);

    shift_elements_forwards(vector, idx);
    void* target_addr = offset(blk->data, local_index * stride);
    memcpy(target_addr, element, stride);

    register_addition(vector);
}

void* dvect_reserve(DynVector* vector) {
    if (!ensure_capacity(vector, vector->size + 1))
        return NULL;

    void* dst = offset(vector->current->data, vector->next_insert_index * vector->stride);
    register_addition(vector);
    return dst;
}

bool dvect_remove(DynVector* vector, u64 idx, void* out) {
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

bool dvect_pop(DynVector* vector, void* out) {
    if(!dvect_peek(vector, out))
        return FALSE;

    register_removal(vector);

    return TRUE;
}

bool dvect_peek(DynVector* vector, void* out) {
    u64 size = vector->size;
    if (size == 0)
        return FALSE;
    u64 stride = vector->stride;

    u64 target_index = vector->next_insert_index - 1;
    struct vector_block* target_blk = vector->current;
    if(vector->next_insert_index == 0) {
        target_blk = target_blk->prev;
        target_index = target_blk->capacity - 1;
    }

    void* src = offset(target_blk->data, target_index * stride);

    if (out)
        memcpy(out, src, stride);
    return TRUE;
}

bool dvect_get(DynVector* vector, u64 index, void* out) {
    u64 size = vector->size;
    if (index >= size)
        return FALSE;

    u64 stride = vector->stride;

    u64 local_index;
    struct vector_block* blk = get_block_from_index(vector, index, &local_index);

    if (out)
        memcpy(out, offset(blk->data, local_index * stride), stride);
    return TRUE;
}

void* dvect_ref(DynVector* vector, u64 index) {
    u64 size = vector->size;
    if (index >= size)
        return NULL;

    u64 stride = vector->stride;
    u64 local_index;
    struct vector_block* blk = get_block_from_index(vector, index, &local_index);

    return offset(blk->data, local_index * stride);
}
