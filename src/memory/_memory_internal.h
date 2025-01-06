//
// Created by bmorino on 06/01/2025.
//

#ifndef _MEMORY_INTERNAL_H
#define _MEMORY_INTERNAL_H

#include "definitions.h"
#include "mem_tags.h"

i64 register_block(void* blk, u64 size, enum MemoryBlockTag tag);
void unregister_block(i64 idx);
void register_alloc(i64 arena_idx, i32 start, i32 end, i32 tags);

#endif /* ! _MEMORY_INTERNAL_H */
