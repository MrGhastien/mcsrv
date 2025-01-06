/**
 * @file
 *
 * A simple linear memory allocator.
 */
#ifndef ARENA_H
#define ARENA_H

#include "definitions.h"
#include "mem_tags.h"

/**
   Simple linear allocator.

   The allocated memory is contiguous, but is limited.
 */
typedef struct arena {
    void* block;
    u64 capacity;
    u64 length;
    u64 saved_length;
    i64 stats_index;
    bool logging;
} Arena;

/**
 * Creates an arena allocator of the specified size.
 *
 * @param size The number of bytes to allocate for the arena.
 * @param tag
 * @return The new arena allocator.
 */
Arena arena_create(u64 size, enum MemoryBlockTag tag);
/**
 * Creates an arena allocator of the specified size, without logging anything.
 *
 * This is functionally the same as arena_create(u64), except is does not log any trace messages.
 *
 * @param size The number of bytes to allocate for the arena.
 * @param tag
 * @return The new arena allocator.
 */
Arena arena_create_silent(u64 size, enum MemoryBlockTag tag);
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
 * @param tags
 */
void* arena_allocate(Arena* arena, u64 bytes, enum AllocTag tags);
/**
 * Allocates memory in an arena and fills it with 0.
 *
 * If no memory is available, abort() is called.
 *
 * @param arena The arena to use to allocate memory.
 * @param bytes The amount of bytes to allocate.
 * @param tags
 */
void* arena_callocate(Arena* arena, u64 bytes, enum AllocTag tags);

void* arena_allocate_aligned(Arena* arena, u64 bytes);
void* arena_callocate_aligned(Arena* arena, u64 bytes);
/**
 * Frees the specified amount of bytes from an arena.
 *
 * If the amount of bytes is greater than the length of the allocated memory
 * of the arena, it is silently clamped all the memory is freed
 */
void arena_free(Arena* arena, u64 bytes);
/**
 * Frees the specified amount of bytes from an arena.
 *
 * If the amount of bytes is greater than the length of the allocated memory
 * of the arena, it is silently clamped all the memory is freed
 */
void arena_free_ptr(Arena* arena, void* ptr);

/**
 * Saves the current pointer to available memory.
 *
 * If another pointer is saved, it is discarded and replaced by the current one.
 *
 * @param arena The arena of which to save the current pointer.
 */
void arena_save(Arena* arena);
/**
 * Restores the available memory pointer that was previously saved.
 *
 * Note that calling this function before saving any available memory pointer simply
 * restores the pointer to the start of the underlying memory, i.e. frees all memory.
 *
 * @param arena The arena of which to restore the available memory pointer.
 */
void arena_restore(Arena* arena);

/**
 * Returns the number of bytes saved.
 *
 * After restoring the saved pointer, the arena will have the same amount
 * of allocated bytes as the number returned by this function.
 *
 * @param arena The arena from which to get the saved pointer.
 * @return The amount of saved bytes.
 */
u64 arena_recent_length(Arena* arena);
/**
 * Returns the saved available memory pointer.
 *
 * @param arena The arena from which to get the saved pointer.
 * @return The saved pointer.
 */
void* arena_recent_pos(Arena* arena);

/**
 * Indicates whether two arenas share the same memory block or not.
 *
 * @param[in] a The first arena.
 * @param[in] b The second arena.
 * @return @ref TRUE if both arenas share the same memory block, @ref FALSE otherwise.
 */
static inline bool arena_is_mem_shared(const Arena* a, const Arena* b) {
    return a->block == b->block;
}

void memory_dump_stats(void);

#endif /* ! ARENA_H */
