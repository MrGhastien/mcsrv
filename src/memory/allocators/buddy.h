/**
 * @file buddy.h
 * @author Bastien Morino
 * @brief Buddy allocator.
 *
 * @see https://wikipedia.org/wiki/Buddy_memory_allocation
 */

#ifndef BUDDY_H
#define BUDDY_H

#include "memory/_memory_internal.h"
#include "memory/allocators/pool.h"

typedef struct BuddyBlock BuddyBlock;

typedef struct BuddyAllocator {
    PoolAllocator metas;
    memory_chain chain;
    BuddyBlock* head;
} BuddyAllocator;

/**
 * Initializes a buddy allocator.
 *
 * The @p size parameter is rounder up to the nearest power of two, starting at 2⁵ (32).
 *
 * @param[out] alloc A pointer to the allocator object to initialize.
 * @param[in] size The total size of the allocator, rounded up to the nearest power of two.
 * @param[in] tag A tag to apply to the underlying memory allocation(s).
 */
void buddy_init(BuddyAllocator* alloc, u64 size, enum MemoryChainTag tag);

/**
 * Tears down a buddy allocator.
 *
 * @param[inout] alloc A pointer to the allocator object to tear down.
 */
void buddy_destroy(BuddyAllocator* alloc);

/**
 * Allocates memory from a buddy allocator.
 *
 * @param[inout] alloc The allocator to use.
 * @param[in] size The size of the allocation to make.
 * @returns A pointer to the allocated memory, or `NULL` if a problem occurred.
 */
void* buddy_alloc(BuddyAllocator* alloc, u64 size);
/**
 * Frees memory from a buddy allocator.
 *
 * @param[inout] alloc The allocator to use.
 * @param[in] ptr A pointer to the memory to free. Must not be @p NULL.
 */
void buddy_free(BuddyAllocator* alloc, void* ptr);

#endif /* ! BUDDY_H */
