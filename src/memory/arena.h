#ifndef ARENA_H
#define ARENA_H

#include "definitions.h"
#include <stddef.h>

/**
   Simple linear allocator.

   The allocated memory is contiguous, but is limited.
 */
typedef struct arena {
    void* block;
    u64 capacity;
    u64 length;
    u64 saved_length;
    bool logging;
} Arena;

Arena arena_create(u64 size);
Arena arena_create_silent(u64 size);
void arena_destroy(Arena* arena);

void* arena_allocate(Arena* arena, u64 bytes);
void* arena_callocate(Arena* arena, u64 bytes);
void arena_free(Arena* arena, u64 bytes);
void arena_free_ptr(Arena* arena, void* ptr);

void arena_save(Arena* arena);
void arena_restore(Arena* arena);

u64 arena_recent_length(Arena* arena);
void* arena_recent_pos(Arena* arena);

#endif /* ! ARENA_H */
