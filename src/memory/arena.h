#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>

/**
   Simple linear allocator.

   The allocated memory is contiguous, but is limited.
 */
typedef struct arena {
    void* block;
    size_t capacity;
    size_t length;
    size_t saved_length;
} Arena;

Arena arena_create(size_t size);
void arena_destroy(Arena* arena);

void* arena_allocate(Arena* arena, size_t bytes);
void* arena_callocate(Arena* arena, size_t bytes);
void arena_free(Arena* arena, size_t bytes);
void arena_free_ptr(Arena* arena, void* ptr);

void arena_save(Arena* arena);
void arena_restore(Arena* arena);

size_t arena_recent_length(Arena* arena);
void* arena_recent_pos(Arena* arena);

#endif /* ! ARENA_H */
