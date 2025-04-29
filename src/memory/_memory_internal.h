//
// Created by bmorino on 06/01/2025.
//

#ifndef _MEMORY_INTERNAL_H
#define _MEMORY_INTERNAL_H

#include "definitions.h"
#include "mem_tags.h"
#include "utils/bitwise.h"

struct memory_block {
    enum AllocationType type;
    u64 capacity;
    void* start;
    i32 next;
    i32 prev;
};

struct memory_chain {
    enum MemoryChainTag tag;
    u32 block_count;
    i32 head;
    i32 tail;

    i32 next_chain;
    i32 prev_chain;
};

typedef i32 memory_block;
typedef i32 memory_chain;

#define INVALID_CHAIN ((memory_chain)-1)
#define INVALID_BLOCK ((memory_block)-1)

void register_alloc(const memory_block block, u64 start, u64 end, enum AllocTag tag);
void unregister_alloc(const memory_block block, u64 start);

/**
 * Creates a new memory block chain, possibly linked to another one.
 *
 * @param[in] tag A tag used to indicate where this chain is used, solely for statistics.
 * @param[in] prev A pointer to an other memory chain, to link the new chain with it. Can be NULL,
 * in which case the new chain is not linked to any other chain.
 */
memory_chain create_chain(enum MemoryChainTag tag, memory_chain prev);

/**
 * Destroys a memory block chain by freeing all of its blocks.
 *
 * This function destroys all linked chains as well.
 *
 * @param[in] chain A pointer to the chain to destroy.
 */
void destroy_chain(memory_chain chain);

/**
 * Allocates a new memory block, and adds it to the given chain.
 *
 * The @p size parameter is only a hint: The size of the actual allocation may be bigger,
 * but it is guaranteed to be at least as large as @p size.
 *
 * @param[in] capacity The size of the block to allocate.
 * @param[in] chain The chain to add this block to.
 * @return A pointer to the allocated memory, or @ref NULL if the allocation failed.
 */
memory_block alloc_block(u64 capacity, memory_chain chain);
void delete_block(memory_block blk);

memory_block chain_head(memory_chain chain);
memory_block block_next(memory_block block);
u64 block_capacity(memory_block block);
void* block_memory(memory_block block);

static inline bool is_addr_in_block(const void* addr, const memory_block blk) {
    void* mem = block_memory(blk);
    return addr >= mem && addr <= offset(mem, block_capacity(blk));
}

#define STATIC_MEM_BLOCK(name, size, mtag)                                                         \
    static u8 _##name##_static_buf[size];                                                          \
    static struct memory_block name = {                                                            \
        .type = ALLOC_TYPE_STATIC, .capacity = size, .start = _##name##_static_buf}

#endif /* ! _MEMORY_INTERNAL_H */
