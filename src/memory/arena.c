#include "arena.h"
#include "../utils/bitwise.h"

#include <stdlib.h>
#include <string.h>
#define DEFAULT_CAP 64

Arena arena_create() {
    Arena res = {.block = malloc(DEFAULT_CAP), .capacity = DEFAULT_CAP, .length = 0};
    if (!res.block)
        res.capacity = 0;
    return res;
}

void arena_destroy(Arena* arena) {
    free(arena->block);
}

static void ensure_capacity(Arena* arena, size_t capacity) {
    if (capacity <= arena->capacity)
        return;
    size_t new_cap = ceil_two_pow(capacity);
    void* new_block = realloc(arena->block, new_cap);
    if (!new_block)
        return;

    arena->block = new_block;
    arena->capacity = new_cap;
}

void arena_reserve(Arena* arena, size_t bytes) {
    ensure_capacity(arena, bytes);
}

void* arena_allocate(Arena* arena, size_t bytes) {
    if (bytes == 0)
        return offset(arena->block, arena->length);

    size_t final_length = arena->length + bytes;
    ensure_capacity(arena, final_length);

    void* ptr = offset(arena->block, arena->length);
    arena->length = final_length;
    return ptr;
}

void* arena_callocate(Arena* arena, size_t bytes) {
    void* ptr = arena_allocate(arena, bytes);
    return memset(ptr, 0, bytes);
}

void arena_free(Arena* arena, size_t bytes) {
    if (bytes >= arena->length)
        arena_destroy(arena);
    else
        arena->length -= bytes;
}

void arena_free_ptr(Arena *arena, void *ptr) {
    void* end = offset(arena->block, arena->length);
    if(ptr >= end)
        return;
    size_t to_free = end - ptr;

    arena_free(arena, to_free);
}
