//
// Created by bmorino on 31/12/2024.
//

#ifndef _ARRAY_INTERNAL_H
#define _ARRAY_INTERNAL_H

#include "definitions.h"

struct data_block {
    u32 capacity;
    void* data;
    struct data_block* next;
    struct data_block* prev;
};

struct data_block* alloc_block(Arena* arena, u64 capacity, u64 stride);
struct data_block* get_block_from_index(struct data_block* start, u64 index, u64* out_blk_local_index);

#endif /* ! _ARRAY_INTERNAL_H */
