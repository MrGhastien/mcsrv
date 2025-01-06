//
// Created by bmorino on 06/01/2025.
//

#include "_memory_internal.h"
#include "mem_tags.h"

#include "containers/object_pool.h"
#include "containers/vector.h"
#include "definitions.h"
#include "logger.h"
#include "platform/mc_mutex.h"
#include "platform/mc_thread.h"
#include "utils/ansi_codes.h"

#define TRACKER_BUF_SIZE (1 << 24)

struct alloc_track {
    enum AllocTag tag;
    i32 start;
    i32 end;
};

struct blk_track {
    Vector allocs;
    const void* block;
    u64 size;
    enum MemoryBlockTag tag;
};

static const char* ALLOC_TAG_NAMES[] = {
    [ALLOC_TAG_UNKNOWN] = "Unknown",
    [ALLOC_TAG_VECTOR] = "Vector",
    [ALLOC_TAG_POOL] = "Object pool",
    [ALLOC_TAG_STRING] = "String",
    [ALLOC_TAG_DICT] = "Dictionary",
    [ALLOC_TAG_BYTEBUFFER] = "ByteBuffer",
    [ALLOC_TAG_NBT] = "NBT",
    [ALLOC_TAG_JSON] = "JSON",
};

static const char* BLK_TAG_NAMES[] = {
    [BLK_TAG_UNKNOWN] = "Unknown",
    [BLK_TAG_NETWORK] = "Network",
    [BLK_TAG_EVENT] = "Event",
    [BLK_TAG_REGISTRY] = "Registry",
    [BLK_TAG_PLATFORM] = "Platform",
    [BLK_TAG_MEMORY] = "Memory",
};

static u8 arena_buf[TRACKER_BUF_SIZE];
static Arena stats_arena;
static ObjectPool arenas;
static bool tracker_initialized = FALSE;
static MCMutex stats_mutex;
static MCThreadKey tag_key;

string get_alloc_tag_name(enum AllocTag tag) {
    if (tag >= _ALLOC_TAG_COUNT || tag < ALLOC_TAG_UNKNOWN)
        return str_create_view(NULL);

    return str_create_view(ALLOC_TAG_NAMES[tag]);
}
string get_blk_tag_name(enum MemoryBlockTag tag) {
    if (tag >= _BLK_TAG_COUNT || tag < BLK_TAG_UNKNOWN)
        return str_create_view(NULL);

    return str_create_view(BLK_TAG_NAMES[tag]);
}

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

void memory_stats_init(void) {
    if (tracker_initialized)
        return;
    tracker_initialized = TRUE;
    if (!mcthread_create_attachment(&tag_key)) {
        log_fatal("Failed to initialize the memory instrumentation tool.");
        abort();
    }
    mcmutex_create(&stats_mutex);
    stats_arena = arena_create_from(arena_buf, TRACKER_BUF_SIZE);
    objpool_init_dynamic(&arenas, &stats_arena, 8, sizeof(struct blk_track));
}

i64 register_block(void* blk, u64 size, enum MemoryBlockTag tag) {
    i64 idx;
    mcmutex_lock(&stats_mutex);
    struct blk_track* track = objpool_add(&arenas, &idx);
    track->block = blk;
    track->size = size;
    track->tag = tag;
    vect_init_dynamic(&track->allocs, &stats_arena, 64, sizeof(struct alloc_track));
    mcmutex_unlock(&stats_mutex);
    return idx;
}

void unregister_block(i64 idx) {
    mcmutex_lock(&stats_mutex);
    objpool_remove(&arenas, idx);
    mcmutex_unlock(&stats_mutex);
}

void register_alloc(i64 arena_idx, i32 start, i32 end, enum AllocTag tag) {
    if (arena_idx < 0)
        return;

    if (start >= end) {
        log_error("Memory: The start offset of an allocation must be strictly less than its end !");
        return;
    }

    i32 global_tags = (i64) mcthread_get_data(tag_key) & 0xffffffff;
    struct alloc_track alloc = {
        .start = start,
        .end = end,
        .tag = tag | global_tags,
    };

    mcmutex_lock(&stats_mutex);
    struct blk_track* track = objpool_get(&arenas, arena_idx);
    struct alloc_track* tmp_alloc;
    while ((tmp_alloc = vect_ref(&track->allocs, vect_size(&track->allocs) - 1))) {
        if (tmp_alloc->start == start) {
            tmp_alloc->end = end;
            tmp_alloc->tag = tag;
            mcmutex_unlock(&stats_mutex);
            return;
        }

        if (tmp_alloc->start < start) {
            if(tmp_alloc->end < start)
                log_warn("Memory: A region of memory is allocated after a gap");
            tmp_alloc->end = start;
            break;
        }
        vect_pop(&track->allocs, NULL);
    }
    vect_add(&track->allocs, &alloc);
    mcmutex_unlock(&stats_mutex);
}

struct stat_dump_data {
    u64 total_allocated;
    u64 total_available;
};

static void dump_block_stats(void* ptr, i64 idx, void* user_data) {
    struct stat_dump_data* data = user_data;
    struct blk_track* track = ptr;

    log_infof("Block %li, " ANSI_MAGENTA "%zu" ANSI_RESET " bytes long, starting at " ANSI_CYAN
              "0x%p" ANSI_RESET ", tagged " ANSI_BLUE "%s" ANSI_RESET ":",
              idx,
              track->size,
              track->block,
              get_blk_tag_name(track->tag).base);
    u64 total = 0;
    for (u64 i = 0; i < vect_size(&track->allocs); ++i) {
        struct alloc_track* alloc = vect_ref(&track->allocs, i);
        u64 size = alloc->end - alloc->start;
        log_infof("- Allocated: offset " ANSI_CYAN "%i" ANSI_RESET ", size " ANSI_MAGENTA
                  "%i" ANSI_RESET ", tag " ANSI_BLUE "%s",
                  alloc->start,
                  size,
                  get_alloc_tag_name(alloc->tag).base);

        total += size;
        data->total_allocated += size;
    }
    data->total_available += track->size;
    log_infof("  Block total: " ANSI_MAGENTA "%zu" ANSI_RESET " bytes allocated, " ANSI_MAGENTA
              "%zu" ANSI_RESET " bytes free.",
              total,
              track->size - total);
}

void set_global_tags(i32 tags) {
    i64 long_tags = tags;

    mcthread_attach_data(tag_key, (void*) long_tags);
}

void memory_dump_stats(void) {
    mcmutex_lock(&stats_mutex);
    log_info("=== MEMORY STATISTICS ===");

    struct stat_dump_data data = {0};
    objpool_foreach(&arenas, &dump_block_stats, &data);

    log_infof("Total: " ANSI_MAGENTA "%zu" ANSI_RESET " bytes allocated, " ANSI_MAGENTA
              "%zu" ANSI_RESET " bytes free in " ANSI_MAGENTA "%zu" ANSI_RESET " block(s).",
              data.total_allocated,
              data.total_available,
              objpool_size(&arenas));

    mcmutex_unlock(&stats_mutex);
}
