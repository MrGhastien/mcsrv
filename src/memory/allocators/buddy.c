#include "buddy.h"
#include "memory/_memory_internal.h"
#include "platform/mem_advice.h"
#include "utils/bitwise.h"

#define MIN_BLOCK_SIZE 5
#define MAX_BLOCK_SIZE 12

typedef struct BuddyBlock {
    u32 power_size;
    bool free;
    // memory_block parent_slab;
} BuddyBlock;

static BuddyBlock* buddy_block_next(BuddyBlock* block) {
    return offset(block, 1ULL << (block->power_size + MIN_BLOCK_SIZE));
}

void _buddy_init(BuddyAllocator* alloc, u64 size, enum MemoryChainTag tag, const char* name) {

    size = ceil_two_pow(size + sizeof(BuddyBlock));

    alloc->chain            = create_chain(tag, name, INVALID_CHAIN);
    memory_block blk        = alloc_block(size, alloc->chain, MEM_ADVICE_RANDOM);
    alloc->head             = block_memory(blk);
    alloc->head->power_size = u64_log2(size) - MIN_BLOCK_SIZE;
    alloc->head->free       = true;
    alloc->tail             = buddy_block_next(alloc->head);
}
void buddy_destroy(BuddyAllocator* alloc) {
    destroy_chain(alloc->chain);
}

static BuddyBlock* block_split(BuddyBlock* block, u64 requested_size) {
    if (!block || requested_size == 0 || !block->free)
        return NULL;

    while ((1ULL << (block->power_size + MIN_BLOCK_SIZE - 1)) > requested_size &&
           block->power_size > 1) {
        u64 split_size    = block->power_size - 1;
        block->power_size = split_size;
        BuddyBlock* next  = buddy_block_next(block);
        next->power_size  = split_size;
        next->free        = true;
    }

    return block;
}

static void block_merge(BuddyAllocator* alloc, BuddyBlock* block) {
    if (!block->free)
        return;

    while (block != alloc->tail) {
        BuddyBlock* next = buddy_block_next(block);
        if (next >= alloc->tail)
            return;
        if (next->power_size != block->power_size || !next->free)
            return;
        /*
        if (next->parent_slab != block->parent_slab)
            return;
        */

        block->power_size++;
    }
}

void* buddy_alloc(BuddyAllocator* alloc, u64 size) {
    u64 actual_size = size + sizeof(BuddyBlock);

    // Search for available block
    BuddyBlock* blk = alloc->head;
    while (blk < alloc->tail &&
           ((1ULL << (blk->power_size + MIN_BLOCK_SIZE)) < actual_size || !blk->free)) {
        blk = buddy_block_next(blk);
    }

    blk = block_split(blk, actual_size);

    return offset(blk, sizeof(BuddyBlock));
}

void buddy_free(BuddyAllocator* alloc, void* ptr) {
    BuddyBlock* blk = offset(ptr, -sizeof(BuddyBlock));
    blk->free       = true;

    block_merge(alloc, blk);
}
