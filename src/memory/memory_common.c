//
// Created by bmorino on 06/01/2025.
//

#include "_memory_internal.h"
#include "mem_tags.h"

#include "allocators/pool.h"
#include "containers/vector.h"
#include "definitions.h"
#include "logger.h"
#include "memory/memory.h"
#include "memory/stats/basic_pool.h"
#include "platform/mc_mutex.h"
#include "platform/mc_thread.h"
#include "platform/platform.h"
#include "utils/ansi_codes.h"
#include "utils/bitwise.h"

#include "allocators/pool.h"
#include "utils/debug.h"

#include <assert.h>
#include <stdlib.h>

#define TRACKER_BUF_SIZE (1 << 26)

struct alloc_track {
    enum AllocTag tag;
    u64 start;
    u64 end;
    i64 tracking_index;
    i64 block_index;
};

static const char* ALLOC_TAG_NAMES[] = {
    [ALLOC_TAG_UNKNOWN] = "Unknown",
    [ALLOC_TAG_VECTOR] = "Vector",
    [ALLOC_TAG_POOL] = "Object pool",
    [ALLOC_TAG_STRING] = "String",
    [ALLOC_TAG_DICT] = "Dictionary",
    [ALLOC_TAG_BYTEBUFFER] = "ByteBuffer",
    [ALLOC_TAG_PACKET] = "Packet",
    [ALLOC_TAG_JSON] = "JSON",
    [ALLOC_TAG_NBT] = "NBT",
    [ALLOC_TAG_WORLD] = "World",
    [ALLOC_TAG_EXTERNAL] = "External",
};

static const char* BLK_TAG_NAMES[] = {
    [BLK_TAG_UNKNOWN] = "Unknown",
    [BLK_TAG_NETWORK] = "Network",
    [BLK_TAG_EVENT] = "Event",
    [BLK_TAG_REGISTRY] = "Registry",
    [BLK_TAG_PLATFORM] = "Platform",
    [BLK_TAG_MEMORY] = "Memory",
    [BLK_TAG_DATA] = "Data",
};

static struct basic_pool block_pool;
static struct basic_pool chain_pool;

static bool tracker_initialized = FALSE;
static MCMutex stats_mutex;

memory_chain create_chain(enum MemoryChainTag tag, memory_chain prev) {
    i32 idx;
    mcmutex_lock(&stats_mutex);
    struct memory_chain* new_chain = basic_pool_alloc(&chain_pool, &idx);

    mcmutex_unlock(&stats_mutex);
    *new_chain = (struct memory_chain) {.tag = tag, .prev_chain = prev};
    if(prev >= 0) {
        struct memory_chain* prev_ptr = basic_pool_query(&chain_pool, prev);
        prev_ptr->next_chain = idx;
    }
    return idx;
}
void destroy_chain(memory_chain chain) {

    mcmutex_lock(&stats_mutex);
    struct memory_chain* chain_ptr = basic_pool_query(&chain_pool, chain);

    memory_block blk = chain_ptr->head;
    while(blk) {
        struct memory_block* block_ptr = basic_pool_query(&block_pool, -1);
        i32 next = block_ptr->next;
        delete_block(blk);
        blk = next;
    }
    
    basic_pool_free(&chain_pool, chain_ptr);
    //mc_abort();
    mcmutex_unlock(&stats_mutex);
}

memory_block alloc_block(u64 capacity, memory_chain chain) {
    void* memory = platform_alloc(&capacity);
    assert(memory != NULL);

    mcmutex_lock(&stats_mutex);
    i32 index;
    struct memory_block* block = basic_pool_alloc(&block_pool, &index);
    mcmutex_unlock(&stats_mutex);

    *block = (struct memory_block) {
        .capacity = capacity,
        .next = -1,
        .prev = -1,
        .start = memory,
        .type = ALLOC_TYPE_DYNAMIC,
    };

    struct memory_chain* chain_ptr = basic_pool_query(&chain_pool, chain);

    block->prev = chain_ptr->tail;
    if (chain_ptr->tail >= 0) {
        struct memory_block* tail_ptr = basic_pool_query(&block_pool, chain_ptr->tail);
        tail_ptr->next = index;
    }
    else
        chain_ptr->head = index;
    chain_ptr->tail = index;
    chain_ptr->block_count++;

    return index;
}
void delete_block(memory_block blk) {
    struct memory_block* blk_ptr = basic_pool_query(&block_pool, blk);
    platform_free(blk_ptr->start, blk_ptr->capacity);
    // delete vector !
    basic_pool_free(&block_pool, blk_ptr);
}

memory_block chain_head(memory_chain chain) {
    if(chain < 0)
        return INVALID_BLOCK;

    const struct memory_chain* chain_ptr = basic_pool_query(&chain_pool, chain);
    if(chain_ptr == NULL)
        return INVALID_BLOCK;

    return chain_ptr->head;
}

memory_block block_next(memory_block block) {
    if(block < 0)
        return INVALID_BLOCK;

    const struct memory_block* block_ptr = basic_pool_query(&block_pool, block);
    if(block_ptr == NULL)
        return INVALID_BLOCK;

    return block_ptr->next;
}

void* block_memory(memory_block block) {
    if(block < 0)
        return NULL;

    const struct memory_block* block_ptr = basic_pool_query(&block_pool, block);
    if(block_ptr == NULL)
        return NULL;

    return block_ptr->start;
}

u64 block_capacity(memory_block block) {
    if(block < 0)
        return 0ull;

    const struct memory_block* block_ptr = basic_pool_query(&block_pool, block);
    if(block_ptr == NULL)
        return 0ull;

    return block_ptr->capacity;
}

string get_alloc_tag_name(enum AllocTag tag) {
    if (tag >= _ALLOC_TAG_COUNT || tag < ALLOC_TAG_UNKNOWN)
        return str_view(NULL);

    return str_view(ALLOC_TAG_NAMES[tag]);
}
string get_blk_tag_name(enum MemoryChainTag tag) {
    if (tag >= _BLK_TAG_COUNT || tag < BLK_TAG_UNKNOWN)
        return str_view(NULL);

    return str_view(BLK_TAG_NAMES[tag]);
}

void memory_stats_init(void) {
    if (tracker_initialized)
        return;
    tracker_initialized = TRUE;
    mcmutex_create(&stats_mutex);

    basic_pool_init(&chain_pool, 8, POOL_CHAIN);
    basic_pool_init(&block_pool, 64, POOL_BLOCK);
    //pool_init_static(&allocation_pool, 512, sizeof(memory_block), &allocation_chain);
}

/*

  A: [---------]
  B:      [---------]

  A:      [---------]
  B: [---------]

  A: [--------------]
  B:      [----]

  A:      [----]
  B: [--------------]
 */

/*

static bool check_alloc_overlap(struct alloc_track* new_alloc, i64 block_index) {
    UNUSED(new_alloc);
    UNUSED(block_index);
    return TRUE;

    for (u64 i = 0; i < current_allocs->size; i++) {
        struct alloc_track* tmp_alloc = vect_ref(current_allocs, i);

        if (new_alloc->start >= tmp_alloc->start && new_alloc->start < tmp_alloc->end)
            return FALSE;

        if (tmp_alloc->start >= new_alloc->start && tmp_alloc->start < new_alloc->end)
            return FALSE;
    }
    return TRUE;

}

void register_alloc(const memory_block* block, u64 start, u64 end, enum AllocTag tag) {
    if(block == &allocation_block)
        return;
    if (start >= end) {
        log_error("Memory: The start offset of an allocation must be strictly less than its end !");
        return;
    }

    struct alloc_track alloc = {
        .start = start,
        .end = end,
        .tag = tag,
        .block_index = block->tracking_index,
    };

    mcmutex_lock(&stats_mutex);
    if(!pool_get(&block_pool, block->tracking_index))
        abort();

    if (!check_alloc_overlap(&alloc, block->tracking_index)) {
        log_fatal("New allocation is overlapping other allocations !");
        abort();
    }

    i64 index;
    struct alloc_track* new_alloc = pool_alloc(&allocation_pool, &index);
    alloc.tracking_index = index;
    *new_alloc = alloc;

    mcmutex_unlock(&stats_mutex);
}

struct alloc_search_data {
    u64 start;
    i64 index;
    i64 block_index;
};

static void unregister_single_alloc(void* ptr, i64 idx, void* data) {
    struct alloc_track* alloc = ptr;
    struct alloc_search_data* search_data = data;

    if(search_data->start == alloc->start && alloc->block_index == search_data->block_index)
        search_data->index = idx;
}

void unregister_alloc(const memory_block* block, u64 start) {
    if(!pool_get(&block_pool, block->tracking_index))
        abort();

    struct alloc_search_data data = {.start = start, .index = -1};
    pool_foreach(&allocation_pool, &unregister_single_alloc, &data);
    if(data.index == -1)
        abort();

    pool_free_idx(&allocation_pool, data.index);

    log_warn("Tried to unregister unknown memory allocation");
}

*/
struct stat_dump_data {
    u64 total_allocated;
    u64 total_available;
    Vector alloc_buckets;
};

static void dump_block_stats(const struct memory_block* block, memory_block idx, struct stat_dump_data* data) {
    UNUSED(data);
    log_infof("- Block %li, " ANSI_MAGENTA "%zu" ANSI_RESET " bytes long, starting at " ANSI_CYAN
              "0x%p" ANSI_RESET ":",
              idx,
              block->capacity,
              block->start);
}

static void dump_chain_stats(union basic_pool_elem elem, i32 idx, void* user_data) {
    struct stat_dump_data* data = user_data;
    struct memory_chain* track = elem.chain;

    string tag_name = get_blk_tag_name(track->tag);

    log_infof("Chain %li, " ANSI_MAGENTA "%zu" ANSI_RESET " blocks, tagged " ANSI_BLUE
              "%s" ANSI_RESET ":",
              idx,
              track->block_count,
              cstr(&tag_name));

    memory_block block = track->head;
    struct memory_block* block_ptr;
    while (block) {
        block_ptr = basic_pool_query(&block_pool, block);
        dump_block_stats(block_ptr, block, data);
        block = block_ptr->next;
    }
}

void memory_dump_stats(void) {
    log_info("=== MEMORY STATISTICS ===");

    struct stat_dump_data data = {0};
    basic_pool_foreach(&chain_pool, &dump_chain_stats, &data);

    log_infof("Total: " ANSI_MAGENTA "%zu" ANSI_RESET " bytes allocated, " ANSI_MAGENTA
              "%zu" ANSI_RESET " bytes free in " ANSI_MAGENTA "%zu" ANSI_RESET " block(s).",
              data.total_allocated,
              data.total_available,
              pool_size(&block_pool));
}
