#include "arena.h"
#include "containers/object_pool.h"
#include "containers/vector.h"
#include "logger.h"
#include "utils/bitwise.h"
#include "utils/math.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct alloc_track {
    i32 tags;
    i32 start;
    i32 end;
};

struct blk_track {
    Vector tags;
    void* block;
};

#define TRACKER_BUF_SIZE ((sizeof(bool) + sizeof(struct blk_track)) * 1024)

static u8 arena_buf[TRACKER_BUF_SIZE];
static Arena stats_arena;
static ObjectPool arenas;
static bool tracker_initialized = FALSE;

static Arena arena_create_from(void* buf, u64 size) {
    return (Arena){
        .block = buf,
        .capacity = size,
        .length = 0,
        .saved_length = ~0,
        .logging = FALSE,
    };
}

static void init_stats(void) {
    if (tracker_initialized)
        return;
    tracker_initialized = TRUE;
    stats_arena = arena_create_from(arena_buf, TRACKER_BUF_SIZE);
    objpool_init(&arenas, &stats_arena, 1024, sizeof(Vector*));
}

Arena arena_create(u64 size) {
    if (!tracker_initialized)
        init_stats();

    void* block = malloc(size);
    
    if (!block)
        return (Arena) {0};

    log_tracef("Created arena %p of %zu bytes.", res.block, res.capacity);

    return (Arena) {
        .block = block,
        .capacity = size,
        .length = 0,
        .saved_length = ~0,
        .logging = TRUE,
    };
}

Arena arena_create_silent(u64 size) {
    if (!tracker_initialized)
        init_stats();

    void* block = malloc(size);
    
    if (!block)
        return (Arena) {0};

    return (Arena) {
        .block = block,
        .capacity = size,
        .length = 0,
        .saved_length = ~0,
        .logging = TRUE,
    };
}

void arena_destroy(Arena* arena) {
    free(arena->block);
    if (arena->logging) {
        log_tracef("Destroyed arena %p (%zu / %zu).", arena->block, arena->length, arena->capacity);
    }
    arena->block = NULL;
}

void* arena_allocate(Arena* arena, u64 bytes) {

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

void* arena_callocate(Arena* arena, u64 bytes) {
    void* ptr = arena_allocate(arena, bytes);
    return memset(ptr, 0, bytes);
}

void arena_free(Arena* arena, u64 bytes) {
    if (bytes > arena->length)
        bytes = arena->length;
    arena->length -= bytes;
    if (arena->logging) {
        log_tracef("Freed %zu bytes from %p (%zu/%zu).",
                   bytes,
                   arena->block,
                   arena->length,
                   arena->capacity);
    }
}

void arena_free_ptr(Arena* arena, void* ptr) {
    if (ptr < arena->block || ptr >= offset(arena->block, arena->length))
        return;
    arena->length = ptr - arena->block;
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
    return offset(arena->block, arena->saved_length);
}

u64 arena_recent_length(Arena* arena) {
    return arena->length - arena->saved_length;
}
