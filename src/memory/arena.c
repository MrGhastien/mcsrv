#include "arena.h"
#include "../utils/bitwise.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Arena arena_create(size_t size) {
    Arena res = {.block = malloc(size), .capacity = size, .length = 0, .saved_length = 0};
    if (!res.block)
        res.capacity = 0;

#ifdef DEBUG2
    printf("Create arena %p of %zu bytes.\n", res.block, res.capacity);
#endif

    return res;
}

void arena_destroy(Arena* arena) {
    free(arena->block);
#ifdef DEBUG2
    printf("Destroyed arena %p (%zu / %zu).\n", arena->block, arena->length, arena->capacity);
#endif
    arena->block = NULL;
}

void* arena_allocate(Arena* arena, size_t bytes) {
    size_t remaining = arena->capacity - arena->length;
    if (bytes > remaining) {
#ifdef DEBUG
        fprintf(stderr, "Tried to allocate %zu bytes, but only %zu are available.\n", bytes,
                arena->capacity - arena->length);
#endif
        abort();
        return NULL;
    }

    void* ptr = offset(arena->block, arena->length);
    arena->length += bytes;
#ifdef DEBUG2
    printf("Allocated %zu bytes from %p (%zu/%zu).\n", bytes, arena->block, arena->length, arena->capacity);
#endif
    return ptr;
}

void* arena_callocate(Arena* arena, size_t bytes) {
    void* ptr = arena_allocate(arena, bytes);
    return memset(ptr, 0, bytes);
}

void arena_free(Arena* arena, size_t bytes) {
    if (bytes > arena->length)
        bytes = arena->length;
    arena->length -= bytes;
#ifdef DEBUG2
    printf("Free %zu bytes from %p (%zu/%zu).\n", bytes, arena->block, arena->length, arena->capacity);
#endif
}

void arena_free_ptr(Arena* arena, void* ptr) {
    if (ptr < arena->block || ptr >= offset(arena->block, arena->length))
        return;
    arena->length = ptr - arena->block;
}

void arena_save(Arena* arena) {
    arena->saved_length = arena->length;
}

void arena_restore(Arena* arena) {
    if (arena->length >= arena->saved_length)
        arena_free(arena, arena->length - arena->saved_length);

    arena->saved_length = arena->capacity;
}

void* arena_recent_pos(Arena* arena) {
    return offset(arena->block, arena->saved_length);
}

size_t arena_recent_length(Arena* arena) {
    return arena->length - arena->saved_length;
}
