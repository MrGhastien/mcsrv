#include "buddy.h"
#include "logger.h"
#include "memory/_memory_internal.h"
#include "memory/allocators/pool.h"
#include "memory/mem_tags.h"
#include "platform/platform.h"
#include "utils/bitwise.h"

#define MIN_BLOCK_SIZE (1 << 5)
#define MAX_BLOCK_SIZE 12

struct BuddyBlock {
    u32 size;
    bool free;
    void* memory;
    struct BuddyBlock* next;
};

void buddy_init(BuddyAllocator* alloc, u64 size, enum MemoryChainTag tag) {

    size = ceil_two_pow(size);

    alloc->chain     = create_chain(tag, INVALID_CHAIN);
    memory_block blk = alloc_block(size, alloc->chain);

    pool_init_dynamic(&alloc->metas, 4, sizeof(BuddyBlock), BLK_TAG_MEMORY, alloc->chain);

    BuddyBlock* bblock = pool_alloc(&alloc->metas, NULL);
    *bblock            = (BuddyBlock) {
                   .size   = block_capacity(blk),
                   .free   = TRUE,
                   .memory = block_memory(blk),
    };
    alloc->head = bblock;
}
void buddy_destroy(BuddyAllocator* alloc) {
    destroy_chain(alloc->chain);
}

static BuddyBlock* block_split(BuddyAllocator* alloc, BuddyBlock* block) {
    platform_assert(block, "Buddy Block is NULL!");
    platform_assert(block->free, "Buddy Block is not free!");

    BuddyBlock* new_blk = pool_alloc(&alloc->metas, NULL);
    u64 new_size        = block->size >> 1;
    *new_blk            = (BuddyBlock) {
                   .free   = TRUE,
                   .next   = block->next,
                   .size   = new_size,
                   .memory = offsetu(block->memory, new_size),
    };

    block->next = new_blk;
    block->size = new_size;

    return new_blk;
}

static void block_merge(BuddyAllocator* alloc, BuddyBlock* block) {
    platform_assert(block, "Buddy Block is NULL !");
    platform_assert(block->free, "Buddy Block is not free !");
    platform_assert(block->next, "Buddy Block has no next sibling !");

    BuddyBlock* to_absorb = block->next;
    platform_assert(to_absorb->size == block->size, "Merging buddy blocks would not result in a power of 2.");

    block->size <<= 1;
    block->next = to_absorb->next;

    pool_free(&alloc->metas, to_absorb);
}

static bool can_merge_blocks(BuddyBlock* first) {
    return first->free && first->next && first->next->free;
}

void* buddy_alloc(BuddyAllocator* alloc, u64 size) {
    BuddyBlock* blk = alloc->head;
    while (blk && (blk->size < size || !blk->free)) {
        blk = blk->next;
    }

    if (!blk) {
        log_fatal("TODO: resize buddy allocator");
        platform_abort();
    }

    while((blk->size >> 1) > size && blk->size > MIN_BLOCK_SIZE) {
        block_split(alloc, blk);
    }

    return blk->memory;
}

void buddy_free(BuddyAllocator* alloc, void* ptr) {
    BuddyBlock* blk = offset(ptr, -sizeof(BuddyBlock));
    blk->free       = TRUE;

    while (can_merge_blocks(blk)) {
        block_merge(alloc, blk);
    }
}
