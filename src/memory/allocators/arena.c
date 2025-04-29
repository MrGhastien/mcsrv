#include "arena.h"
#include "logger.h"
#include "utils/bitwise.h"
#include "utils/math.h"
#include "memory/_memory_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


Arena arena_create(u64 size, enum MemoryChainTag tag, memory_chain parent) {
    size = ceil_u64(size, sizeof(uintptr_t));
    memory_chain chain = create_chain(tag, parent);
    memory_block block = alloc_block(size, chain);

    if (!block)
        return (Arena){0};

    log_tracef("Created arena %p of %zu bytes.", block, size);

    return (Arena){
        .chain = chain,
        .block = block_memory(block),
        .capacity = size,
        .length = 0,
        .saved_length = ~0,
        .logging = TRUE,
    };
}

Arena arena_create_silent(u64 size, enum MemoryChainTag tag, memory_chain parent) {
    size = ceil_u64(size, sizeof(uintptr_t));
    memory_chain chain = create_chain(tag, parent);
    memory_block block = alloc_block(size, chain);

    if (!block)
        return (Arena){0};

    return (Arena){
        .chain = chain,
        .block = block_memory(block),
        .capacity = size,
        .length = 0,
        .saved_length = ~0,
        .logging = FALSE,
    };
}

void arena_destroy(Arena* arena) {
    destroy_chain(arena->chain);

    if (arena->logging) {
        log_tracef("Destroyed arena %p (%zu / %zu).", arena->block, arena->length, arena->capacity);
    }
    arena->chain = -1;
    arena->block = NULL;
}

void* arena_allocate(Arena* arena, u64 bytes /*, enum AllocTag tags */) {

    // Alignment
    bytes = ceil_u64(bytes, sizeof(uintptr_t));

    u64 remaining = arena->capacity - arena->length;
    if (bytes > remaining) {
        if (arena->logging)
            log_errorf("Tried to allocate %zu bytes, but only %zu are available.",
                       bytes,
                       arena->capacity - arena->length);
        abort();
        return NULL;
    }

    void* ptr = offset(arena->block, arena->length);
    //register_alloc(arena->chain->head, arena->length, arena->length + bytes, tags);
    arena->length += bytes;
    if (arena->logging) {
        log_tracef("Allocated %zu bytes from %p (%zu/%zu).",
                   bytes,
                   arena->block,
                   arena->length,
                   arena->capacity);
    }

    return ptr;
}

void* arena_callocate(Arena* arena, u64 bytes /*, enum AllocTag tags */) {
    void* ptr = arena_allocate(arena, bytes /*, tags */);
    return memset(ptr, 0, bytes);
}

void arena_free(Arena* arena, u64 bytes) {
    if (bytes > arena->length)
        bytes = arena->length;

    arena->length -= bytes;
    //unregister_alloc(arena->chain->head, arena->length);
    if (arena->logging) {
        log_tracef("Freed %zu bytes from %p (%zu/%zu).",
                   bytes,
                   arena->block,
                   arena->length,
                   arena->capacity);
    }
}

void arena_free_ptr(Arena* arena, void* ptr) {
    if (ptr < arena->block || ptr >= offsetu(arena->block, arena->length))
        return;
    arena_free(arena, ptr - arena->block);
}

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
