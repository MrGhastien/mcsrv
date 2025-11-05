#include "arena.h"
#include "logger.h"
#include "memory/_memory_internal.h"
#include "utils/bitwise.h"
#include "utils/math.h"
#include <platform/platform.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

Arena _arena_create(u64 size, enum MemoryChainTag tag, const char* name, memory_chain parent) {
    size               = ceil_u64(size, sizeof(uintptr_t));
    memory_chain chain = create_chain(tag, name, parent);
    memory_block block = alloc_block(size, chain);

    if (block < 0)
        return (Arena) {0};

    log_tracef("Created arena %p of %zu bytes.", block, size);

    return (Arena) {
        .chain    = chain,
        .current  = block,
        .capacity = size,
        .statik   = FALSE,
    };
}

Arena arena_create_static(u64 size, enum MemoryChainTag tag, memory_chain parent) {
    size               = ceil_u64(size, sizeof(uintptr_t));
    memory_chain chain = create_chain(tag, "", parent);
    memory_block block = alloc_block(size, chain);

    if (block < INVALID_BLOCK)
        return (Arena) {0};

    return (Arena) {
        .chain    = chain,
        .current  = block,
        .capacity = size,
        .statik   = TRUE,
    };
}

void arena_destroy(Arena* arena) {
    destroy_chain(arena->chain);

    if (arena->statik) {
        log_tracef("Destroyed arena %p (%zu / %zu).", arena->block, arena->length, arena->capacity);
    }
    arena->chain = -1;
}

void* arena_allocate(Arena* arena, u64 bytes /*, enum AllocTag tags */) {

    // Alignment
    bytes = ceil_u64(bytes, sizeof(uintptr_t));

    u64 used      = block_used(arena->current);
    u64 remaining = block_capacity(arena->current) - used;
    if (bytes > remaining) {
        if (arena->statik) {
            log_errorf("Tried to allocate %zu bytes, but only %zu are available.",
                       bytes,
                       arena->capacity - arena->length);
            platform_abort();
            return NULL;
        }

        memory_block empty_block = block_next(arena->current);
        if (empty_block == INVALID_BLOCK) {
            u64 new_block_cap = max_u64(bytes, arena->capacity << 1);
            empty_block       = alloc_block(new_block_cap, arena->chain);
            if (empty_block == INVALID_BLOCK)
                return NULL;
            arena->capacity += block_capacity(empty_block);
        }

        used           = 0;
        arena->current = empty_block;
    }

    void* ptr = offset(block_memory(arena->current), used);
    block_set_used(arena->current, used + bytes);
    // register_alloc(arena->chain->head, arena->length, arena->length + bytes, tags);
    arena->length += bytes;
    log_tracef("Allocated %zu bytes from %p (%zu/%zu).",
               bytes,
               arena->block,
               arena->length,
               arena->capacity);

    return ptr;
}

void* arena_callocate(Arena* arena, u64 bytes /*, enum AllocTag tags */) {
    void* ptr = arena_allocate(arena, bytes /*, tags */);
    return memset(ptr, 0, bytes);
}

void arena_free(Arena* arena, u64 bytes) {
    if (bytes > arena->length)
        bytes = arena->length;

    u64 new_size     = arena->length - bytes;
    memory_block blk = arena->current;
    while (bytes > 0) {
        u64 used    = block_used(blk);
        u64 to_free = min_u64(used, bytes);

        block_set_used(blk, used - to_free);
        bytes -= to_free;
        if (to_free == used)
            blk = block_prev(blk);
    }
    arena->current = blk;
    arena->length  = new_size;
    log_tracef(
        "Freed %zu bytes from %p (%zu/%zu).", bytes, arena->block, arena->length, arena->capacity);
}

void arena_clear(Arena* arena) {
    memory_block blk = chain_head(arena->chain);
    while (blk != arena->current) {
        block_set_used(blk, 0);
    }
    block_set_used(arena->current, 0);
    arena->length  = 0;
    arena->current = chain_head(arena->chain);
}
void arena_free_ptr(Arena* arena, void* ptr) {
    memory_block blk = arena->current;
    while (arena->length > 0) {
        ptrdiff_t diff = ptr_diff(ptr, block_memory(blk));
        if (diff >= 0 && (u64) diff < block_used(blk)) {
            u64 used = block_used(blk);
            block_set_used(blk, diff);
            arena->length -= used - diff;
            break;
        } else {
            arena->length -= block_used(blk);
            block_set_used(blk, 0);
            blk = block_prev(blk);
        }
    }
    arena->current = blk;
}

/*
void arena_save(Arena* arena) {
    if (arena->saved_length != ~0ULL) {
        log_debug("Arena pointer save would discard previously saved pointer.");
        return;
    }
    arena->saved_length = arena->length;
}

void arena_restore(Arena* arena) {
    if (arena->length >= arena->saved_length)
        arena_free(arena, arena->length - arena->saved_length);

    arena->saved_length = ~0;
}

void* arena_recent_pos(Arena* arena) {
    return offsetu(arena->block, arena->saved_length);
}

u64 arena_recent_length(Arena* arena) {
    return arena->length - arena->saved_length;
}
*/
