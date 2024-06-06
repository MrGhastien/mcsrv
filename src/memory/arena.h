#ifndef LINALLOC_H
#define LINALLOC_H

#include "../definitions.h"
#include <stddef.h>

/**
   A simple linear allocator.
 */
typedef struct linalloc {
    void* block;
    size_t capacity;
    size_t length;
} Arena;

Arena arena_create();
void arena_reserve(Arena* arena, size_t bytes);
void arena_destroy(Arena* arena);

void* arena_allocate(Arena* arena, size_t bytes);
void* arena_callocate(Arena* arena, size_t bytes);
void arena_free(Arena* arena, size_t bytes);
void arena_free_ptr(Arena* arena, void* ptr);

#endif /* ! LINALLOC_H */
