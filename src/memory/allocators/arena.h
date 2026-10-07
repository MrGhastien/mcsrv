/**
 * @file
 *
 * A simple linear memory allocator.
 */
#ifndef ARENA_H
#define ARENA_H

#include "definitions.h"
#include "memory/_memory_internal.h"
#include "memory/mem_tags.h"

enum ArenaPolicy { ARENA_GROW, ARENA_FIXED };

/**
   Simple linear allocator.

   The allocated memory is contiguous, but is limited.
 */
typedef struct arena {
    memory_chain chain;
    memory_block current_block;
    void* cursor;
    void* block_end;
    u64 capacity;
    enum ArenaPolicy policy;
} Arena;

typedef struct {
    Arena* arena;
    memory_block current_block;
    void* cursor;
    void* block_end;
} ArenaCheckpoint;

/**
 * Creates an arena allocator of the specified size.
 *
 * @param size The number of bytes to allocate for the arena.
 * @param tag
 * @return The new arena allocator.
 */
Arena _arena_create(u64 size, enum MemoryChainTag tag, const char* name, memory_chain parent);
/**
 * Creates an arena allocator of the specified size, without logging anything.
 *
 * This is functionally the same as arena_create(u64), except it does not log any trace messages.
 * This variant is used by the logger to prevent infinite recursions.
 *
 * @param size The number of bytes to allocate for the arena.
 * @param tag
 * @return The new arena allocator.
 */
Arena arena_create_static(void* memory, u64 size);
/**
 * Frees all memory associated with an arena.
 *
 * After being passed to a arena_destroy(Arena) call, the allocator can no longer be used.
 *
 * @param arena The arena to destroy.
 */
void arena_destroy(Arena* arena);

/**
 * Allocates memory in an arena.
 *
 * Allocated memory is not initialized.
 * If no memory is available, abort() is called.
 *
 * @param arena The arena to use to allocate memory.
 * @param bytes The amount of bytes to allocate.
 * @param tags Tags for the allocations. Used by memory instrumentation.
 */
void* arena_allocate(Arena* arena, u64 bytes /*, enum AllocTag tags */);
/**
 * Allocates memory in an arena and fills it with 0.
 *
 * If no memory is available, abort() is called.
 *
 * @param arena The arena to use to allocate memory.
 * @param bytes The amount of bytes to allocate.
 * @param tags Tags for the allocations. Used by memory instrumentation.
 */
void* arena_callocate(Arena* arena, u64 bytes /*, enum AllocTag tags */);

void* arena_allocate_aligned(Arena* arena, u64 bytes);
void* arena_callocate_aligned(Arena* arena, u64 bytes);
/**
 * Frees the specified amount of bytes from an arena.
 *
 * If the amount of bytes is greater than the length of the allocated memory
 * of the arena, it is silently clamped all the memory is freed
 */
void arena_free(Arena* arena, u64 bytes);

void arena_clear(Arena* arena);
/**
 * Frees the specified amount of bytes from an arena.
 *
 * If the amount of bytes is greater than the length of the allocated memory
 * of the arena, it is silently clamped all the memory is freed
 */
void arena_free_ptr(Arena* arena, void* ptr);

/**
 * Indicates whether two arenas share the same memory block or not.
 *
 * @param[in] a The first arena.
 * @param[in] b The second arena.
 * @return @ref true if both arenas share the same memory block, @ref false otherwise.
 */
static inline bool arena_is_mem_shared(const Arena* a, const Arena* b) {
    return a->chain == b->chain;
}

void memory_dump_stats(void);

#define arena_create(size, tag, parent) _arena_create(size, tag, (__FILE__ ":" MACRO_STRINGIZE(__LINE__)), parent) 

ArenaCheckpoint arena_create_checkpoint(Arena* arena);
void arena_restore_checkpoint(ArenaCheckpoint point);

#define ARENA_SCOPE(a) \
    for (ArenaCheckpoint _t = arena_create_checkpoint(a), *_o = &_t; _o; arena_restore_checkpoint(_t), _o = NULL)

#endif /* ! ARENA_H */
