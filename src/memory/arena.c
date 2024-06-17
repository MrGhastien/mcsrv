#include "arena.h"
#include "utils/bitwise.h"
#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Arena arena_create(size_t size) {
    Arena res = {.block = malloc(size), .capacity = size, .length = 0, .saved_length = 0};
    if (!res.block)
        res.capacity = 0;

    log_tracef("Created arena %p of %zu bytes.", res.block, res.capacity);

    return res;
}

void arena_destroy(Arena* arena) {
    free(arena->block);
    log_tracef("Destroyed arena %p (%zu / %zu).", arena->block, arena->length, arena->capacity);
    arena->block = NULL;
}

void* arena_allocate(Arena* arena, size_t bytes) {
    size_t remaining = arena->capacity - arena->length;
    if (bytes > remaining) {
        log_errorf("Tried to allocate %zu bytes, but only %zu are available.", bytes,
                arena->capacity - arena->length);
        abort();
        return NULL;
    }

    void* ptr = offset(arena->block, arena->length);
    arena->length += bytes;
    log_tracef("Allocated %zu bytes from %p (%zu/%zu).", bytes, arena->block, arena->length, arena->capacity);
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
    log_tracef("Free %zu bytes from %p (%zu/%zu).", bytes, arena->block, arena->length, arena->capacity);
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
