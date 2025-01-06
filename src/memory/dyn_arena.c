#include "dyn_arena.h"
#include "../utils/bitwise.h"
#include "arena.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define BLOCK_SIZE 4096

static void ensure_capacity(DynamicArena* arena, size_t capacity) {
    if (capacity <= arena->block_capacity)
        return;
    size_t new_cap = ceil_two_pow(capacity);
    Arena* new_block_array = realloc(arena->blocks, new_cap * sizeof(Arena));
    if (!new_block_array)
        return;

    arena->blocks = new_block_array;
    arena->block_capacity = new_cap;
}

static Arena* add_block(DynamicArena* arena, size_t size) {
    ensure_capacity(arena, arena->block_count + 1);
    Arena* blk = &arena->blocks[arena->block_count];
    *blk = arena_create(BLOCK_SIZE * ((BLOCK_SIZE - 1 + size) / BLOCK_SIZE));
    arena->block_count++;
#ifdef DEBUG
    printf("Added block with capacity %zu bytes.\n", blk->capacity);
#endif
    return blk;
}

static void remove_block(DynamicArena* arena, size_t idx) {
    arena_destroy(&arena->blocks[idx]);
    memmove(arena->blocks + idx, arena->blocks + idx + 1, arena->block_count - idx - 1);
    arena->block_count--;
#ifdef DEBUG
    puts("Removed block");
#endif
}

static long get_enclosing_block(const DynamicArena* arena, void* ptr) {
    for (size_t i = 0; i < arena->block_count; i++) {
        Arena* blk = &arena->blocks[i];
        if (ptr >= blk->block && ptr < offset(blk->block, blk->length))
            return i;
    }
    return -1;
}

static Arena* find_suitable_block(const DynamicArena* arena, size_t size) {
    Arena* suitable = NULL;
    for (size_t i = 0; i < arena->block_count; i++) {
        Arena* block = &arena->blocks[i];
        if (block->capacity - block->length >= size) {
            suitable = block;
            break;
        }
    }
    return suitable;
}

#ifdef DEBUG
static size_t total_size(const DynamicArena* arena) {
    size_t res = 0;
    for (size_t i = 0; i < arena->block_count; i++) {
        res += arena->blocks[i].length;
    }
    return res;
}
#endif

DynamicArena dynarena_create() {
    DynamicArena res = {.blocks = malloc(sizeof(Arena) * 4), .block_capacity = 4, .block_count = 0};

    return res;
}

void dynarena_destroy(DynamicArena* arena) {
    for (size_t i = 0; i < arena->block_count; i++) {
        arena_destroy(&arena->blocks[i]);
    }
    free(arena->blocks);
}

void dynarena_reserve(DynamicArena* arena, size_t bytes) {
    Arena* blk = find_suitable_block(arena, bytes);
    if (blk == NULL)
        add_block(arena, bytes);
}

void* dynarena_allocate(DynamicArena* arena, size_t bytes) {
    if (bytes == 0)
        return NULL;

    Arena* suitable = find_suitable_block(arena, bytes);
    if (suitable == NULL)
        suitable = add_block(arena, bytes);

    void* ptr = arena_allocate(suitable, bytes, 0);

#ifdef DEBUG
    printf("Allocated %zu bytes, now %zu bytes.\n", bytes, total_size(arena));
#endif

    return ptr;
}

void* dynarena_callocate(DynamicArena* arena, size_t bytes) {
    void* ptr = dynarena_allocate(arena, bytes);
    return memset(ptr, 0, bytes);
}

void dynarena_free(DynamicArena* arena, size_t bytes) {
    Arena* blk = &arena->blocks[arena->block_count];
    arena_free(blk, bytes);
    if (blk->length == 0)
        remove_block(arena, arena->block_count - 1);

#ifdef DEBUG
    printf("Freed %zu bytes, now %zu bytes.\n", bytes, total_size(arena));
#endif
}

void dynarena_free_ptr(DynamicArena* arena, void* ptr) {
    long idx = get_enclosing_block(arena, ptr);
    if (idx == -1)
        return;

    Arena* blk = &arena->blocks[idx];
    arena_free_ptr(blk, ptr);
    if (blk->length == 0)
        remove_block(arena, arena->block_count - 1);
}
