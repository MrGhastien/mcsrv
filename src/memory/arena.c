#include "arena.h"
#include "../utils/bitwise.h"

#include <stdlib.h>
#define DEFAULT_CAP 64

Arena arena_create() {
    Arena res = {
        .block = malloc(DEFAULT_CAP),
        .capacity = DEFAULT_CAP,
        .length = 0
    };
    if(!res.block)
        res.capacity = 0;
    return res;
}

void arena_destroy(Arena* arena) {
    free(arena->block);
}

void* arena_allocate(Arena* arena, size_t bytes) {
    size_t final_length = arena->length + bytes;
    if(final_length > arena->capacity) {
        size_t new_cap = ceil_two_pow(final_length);
        void* new_block = realloc(arena->block, new_cap);
        if(!new_block)
            return NULL;

        arena->block = new_block;
        arena->capacity = new_cap;
    }
    void* ptr = arena->block + arena->length;
    arena->length = final_length;
    return ptr;
}

void arena_free(Arena* arena, size_t bytes) {
    if (bytes >= arena->length)
        arena_destroy(arena);
    else
        arena->length -= bytes;
}
