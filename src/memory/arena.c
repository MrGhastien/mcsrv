#include "arena.h"
#include "containers/object_pool.h"
#include "containers/vector.h"
#include "logger.h"
#include "mem_tags.h"
#include "platform/mc_mutex.h"
#include "utils/bitwise.h"
#include "utils/math.h"

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
    const void* block;
    u64 size;
};

#define TRACKER_BUF_SIZE (1 << 24)

static u8 arena_buf[TRACKER_BUF_SIZE];
static Arena stats_arena;
static ObjectPool arenas;
static bool tracker_initialized = FALSE;
static MCMutex stats_mutex;

static Arena arena_create_from(void* buf, u64 size) {
    return (Arena){
        .block = buf,
        .capacity = size,
        .length = 0,
        .stats_index = -1,
        .saved_length = ~0,
        .logging = FALSE,
    };
}

static void init_stats(void) {
    if (tracker_initialized)
        return;
    tracker_initialized = TRUE;
    mcmutex_create(&stats_mutex);
    stats_arena = arena_create_from(arena_buf, TRACKER_BUF_SIZE);
    objpool_init(&arenas, &stats_arena, 1024, sizeof(struct blk_track));
}

static i64 register_block(const void* blk, u64 size) {
    i64 idx;
    mcmutex_lock(&stats_mutex);
    struct blk_track* track = objpool_add(&arenas, &idx);
    track->block = blk;
    track->size = size;
    vect_init_dynamic(&track->tags, &stats_arena, 64, sizeof(struct alloc_track));
    mcmutex_unlock(&stats_mutex);
    return idx;
}

static void unregister_block(i64 idx) {
    mcmutex_lock(&stats_mutex);
    objpool_remove(&arenas, idx);
    mcmutex_unlock(&stats_mutex);
}

static void register_alloc(i64 arena_idx, i32 start, i32 end, i32 tags) {
    if(arena_idx == -1)
        return;
    mcmutex_lock(&stats_mutex);
    struct blk_track* track = objpool_get(&arenas, arena_idx);

    struct alloc_track alloc = {
        .start = start,
        .end = end,
        .tags = tags,
    };

    vect_add(&track->tags, &alloc);
    mcmutex_unlock(&stats_mutex);
}

static void dump_block_stats(void* ptr, i64 idx) {
    struct blk_track* track = ptr;

    log_infof("Block %li, %zu bytes long, starting at 0x%p:", idx, track->size, track->block);
    u64 total = 0;
    for (u64 i = 0; i < vect_size(&track->tags); ++i) {
        struct alloc_track* alloc = vect_ref(&track->tags, i);
        u64 size = alloc->end - alloc->start;
        Arena scratch = stats_arena;
        string tag_names = get_tag_names(alloc->tags, &scratch);
        log_infof("- Allocated: offset %i, size %i, type: %s", alloc->start, size, str_printable_buffer(&tag_names));

        total += size;
    }
    log_infof("+ Total: %zu bytes allocated, %zu bytes free.", total, track->size - total);
}

void memory_dump_stats(void) {
    mcmutex_lock(&stats_mutex);
    log_info("=== MEMORY STATISTICS ===");
    log_infof("There are %zu allocated blocks of memory", objpool_size(&arenas));

    objpool_foreach(&arenas, &dump_block_stats);
    mcmutex_unlock(&stats_mutex);
}

Arena arena_create(u64 size) {
    if (!tracker_initialized)
        init_stats();

    void* block = malloc(size);

    if (!block)
        return (Arena){0};

    i64 idx = register_block(block, size);
    log_tracef("Created arena %p of %zu bytes.", res.block, res.capacity);

    return (Arena){
        .block = block,
        .capacity = size,
        .length = 0,
        .saved_length = ~0,
        .stats_index = idx,
        .logging = TRUE,
    };
}

Arena arena_create_silent(u64 size) {
    if (!tracker_initialized)
        init_stats();

    void* block = malloc(size);

    if (!block)
        return (Arena){0};

    i64 idx = register_block(block, size);

    return (Arena){
        .block = block,
        .capacity = size,
        .length = 0,
        .saved_length = ~0,
        .stats_index = idx,
        .logging = TRUE,
    };
}

void arena_destroy(Arena* arena) {
    free(arena->block);
    if (arena->logging) {
        log_tracef("Destroyed arena %p (%zu / %zu).", arena->block, arena->length, arena->capacity);
    }
    arena->block = NULL;

    unregister_block(arena->stats_index);
}

void* arena_allocate(Arena* arena, u64 bytes, i32 tags) {

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
    register_alloc(arena->stats_index, arena->length, arena->length + bytes, tags);
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

void* arena_callocate(Arena* arena, u64 bytes, i32 tags) {
    void* ptr = arena_allocate(arena, bytes, tags);
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
