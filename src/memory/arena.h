#ifndef LINALLOC_H
#define LINALLOC_H

#include "../definitions.h"
#include <stddef.h>

typedef struct linalloc {
    void* block;
    size_t capacity;
    size_t length;
} Arena;

Arena arena_create();
void arena_destroy(Arena* arena);

void* arena_allocate(Arena* arena, size_t bytes);
void arena_free(Arena* arena, size_t bytes);

#endif /* ! LINALLOC_H */
