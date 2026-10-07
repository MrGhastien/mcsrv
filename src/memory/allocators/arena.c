#include "arena.h"
#include "logger.h"
#include "memory/_memory_internal.h"
#include "platform/mem_advice.h"
#include "platform/platform.h"
#include "utils/bitwise.h"
#include "utils/math.h"

#include <stdint.h>
#include <string.h>
#include <sys/types.h>

Arena _arena_create(u64 size, enum MemoryChainTag tag, const char* name, memory_chain parent) {
    size               = ceil_u64(size, sizeof(uintptr_t));
    memory_chain chain = create_chain(tag, name, parent);
    memory_block block = alloc_block(size, chain, MEM_ADVICE_SEQUENTIAL);

    if (block < 0)
        return (Arena){0};

    log_tracef("Created arena (chain: %i) of %zu bytes.", chain, size);

    return (Arena){
        .chain         = chain,
        .current_block = block,
        .cursor        = block_memory(block),
        .block_end     = offset(block_memory(block), block_capacity(block)),
        .capacity      = block_capacity(block),
        .policy        = ARENA_GROW,
    };
}

Arena _arena_create_static(
    void* memory, u64 size, enum MemoryChainTag tag, const char* name, memory_chain parent) {
    size               = ceil_u64(size, sizeof(uintptr_t));
    memory_chain chain = create_chain(tag, name, parent);
    memory_block block = declare_static_block(memory, size, chain);

    if (block < 0)
        return (Arena){0};

    log_tracef("Created arena (chain: %i) of %zu bytes.", chain, size);

    return (Arena){
        .chain         = chain,
        .current_block = block,
        .cursor        = block_memory(block),
        .block_end     = offset(block_memory(block), block_capacity(block)),
        .capacity      = block_capacity(block),
        .policy        = ARENA_FIXED,
    };
}

void arena_destroy(Arena* arena) {
    destroy_chain(arena->chain);

    log_tracef("Destroyed arena %p (%zu / %zu).", arena->chain, arena->length, arena->capacity);

    arena->chain         = INVALID_CHAIN;
    arena->capacity      = 0;
    arena->current_block = INVALID_BLOCK;
    arena->block_end     = NULL;
    arena->cursor        = NULL;
}

void* arena_allocate(Arena* arena, u64 bytes /*, enum AllocTag tags */) {

    // Alignment
    bytes = ceil_u64(bytes, sizeof(uintptr_t));

    u64 remaining = ptr_diff(arena->block_end, arena->cursor);
    if (bytes > remaining) {
        // Find the next large enough block, or allocate a new one.
        memory_block empty_block = block_next(arena->current_block);
        if (empty_block == INVALID_BLOCK) {

            if (arena->policy == ARENA_FIXED) {
                log_errorf("Maximum capacity of fixed arena reached: Tried to allocate %zu bytes, "
                           "but only %zu are available.",
                           bytes,
                           remaining);
                return NULL;
            }

            u64 new_block_cap = max_u64(bytes, arena->capacity >> 1);
            empty_block       = alloc_block(new_block_cap, arena->chain, MEM_ADVICE_SEQUENTIAL);
            if (empty_block == INVALID_BLOCK)
                return NULL;
            arena->capacity += block_capacity(empty_block);
        }

        block_set_used(arena->current_block, ptr_diff(arena->cursor, block_memory(arena->current_block)));
        arena->current_block = empty_block;
        arena->cursor        = block_memory(empty_block);
        arena->block_end     = offset(block_memory(empty_block), block_capacity(empty_block));
    }

    void* ptr = arena->cursor;
    void* new_cursor = offset(arena->cursor, bytes);
    arena->cursor = new_cursor;
    log_tracef("Allocated %zu bytes from %i (%zu/%zu).",
               bytes,
               arena->chain,
               arena->length,
               arena->capacity);

    return ptr;
}

void* arena_callocate(Arena* arena, u64 bytes /*, enum AllocTag tags */) {
    void* ptr = arena_allocate(arena, bytes /*, tags */);
    return memset(ptr, 0, bytes);
}

void arena_free(Arena* arena, u64 bytes) {
    bytes = ceil_u64(bytes, sizeof(uintptr_t));

    u64 used = ptr_diff(arena->cursor, block_memory(arena->current_block));
    bytes    = min_u64(bytes, used);

    arena->cursor = offset(arena->cursor, -bytes);

    log_tracef(
        "Freed %zu bytes from %i (%zu/%zu).", bytes, arena->chain, arena->length, arena->capacity);
}

void arena_clear(Arena* arena) {
    memory_block blk = chain_head(arena->chain);
    while (blk != arena->current_block) {
        block_set_used(blk, 0);
        blk = block_next(blk);
    }
    block_set_used(arena->current_block, 0);
    arena->current_block = chain_head(arena->chain);
    arena->cursor        = block_memory(arena->current_block);
    arena->block_end     = offset(arena->cursor, block_capacity(arena->current_block));
}

static bool block_contains(void* a, void* b, void* ptr) {
    return (uintptr_t) a <= (uintptr_t) ptr && (uintptr_t) ptr < (uintptr_t) b;
}

void arena_free_ptr(Arena* arena, void* ptr) {
    if (block_contains(block_memory(arena->current_block), arena->cursor, ptr)) {
        arena->cursor = ptr;
        return;
    }

    memory_block old_current = arena->current_block;
    memory_block blk         = chain_head(arena->chain);
    while (blk != old_current) {
        void* blk_memory   = block_memory(blk);
        void* blk_used_end = offset(blk_memory, block_used(blk));
        if (block_contains(blk_memory, blk_used_end, ptr)) {
            arena->current_block = blk;
            arena->cursor        = ptr;
            arena->block_end     = offset(blk_memory, block_capacity(blk));
            block_set_used(blk, ptr_diff(ptr, blk_memory));

            memory_block next_blk = block_next(blk);
            while (next_blk != old_current) {
                block_set_used(next_blk, 0);
                advise_block(next_blk, MEM_ADVICE_UNUSED);
                next_blk = block_next(next_blk);
            }
            break;
        } else {
            blk = block_next(blk);
        }
    }

    if (blk == old_current) {
        log_fatal("Double free detected");
        platform_abort();
    }
}

ArenaCheckpoint arena_create_checkpoint(Arena* arena) {
    return (ArenaCheckpoint){
        .arena         = arena,
        .current_block = arena->current_block,
        .cursor        = arena->cursor,
        .block_end     = arena->block_end,
    };
}

void arena_restore_checkpoint(ArenaCheckpoint point) {
    Arena* arena = point.arena;

    memory_block blk = arena->current_block;
    while (point.current_block != blk) {
        block_set_used(blk, 0);
        advise_block(blk, MEM_ADVICE_UNUSED);
        blk = block_prev(blk);
        if (blk == INVALID_BLOCK) {
            log_fatal("Arena checkpoint is after current state");
            platform_abort();
        }
    }

    arena->current_block = point.current_block;
    arena->cursor        = point.cursor;
    arena->block_end = point.block_end;
    ptrdiff_t used = ptr_diff(point.cursor, block_memory(point.current_block));
    block_set_used(point.current_block, used);
}

void arena_check(Arena* arena) {
    platform_assert((uintptr_t) arena->cursor < (uintptr_t) arena->block_end, "Cursor is after the end of the block");
}
