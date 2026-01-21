//
// Created by bmorino on 06/01/2025.
//

#include "_memory_internal.h"
#include "mem_tags.h"

#include "allocators/pool.h"
#include "containers/vector.h"
#include "definitions.h"
#include "logger.h"
#include "memory/allocators/arena.h"
#include "memory/memory.h"
#include "memory/stats/basic_pool.h"
#include "platform/mc_mutex.h"
#include "platform/platform.h"
#include "utils/ansi_codes.h"
#include "utils/string.h"

#include "allocators/pool.h"

#include <assert.h>
#include <stdio.h>
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
    [ALLOC_TAG_UNKNOWN]    = "Unknown",
    [ALLOC_TAG_VECTOR]     = "Vector",
    [ALLOC_TAG_POOL]       = "Object pool",
    [ALLOC_TAG_STRING]     = "String",
    [ALLOC_TAG_DICT]       = "Dictionary",
    [ALLOC_TAG_BYTEBUFFER] = "ByteBuffer",
    [ALLOC_TAG_PACKET]     = "Packet",
    [ALLOC_TAG_JSON]       = "JSON",
    [ALLOC_TAG_NBT]        = "NBT",
    [ALLOC_TAG_WORLD]      = "World",
    [ALLOC_TAG_EXTERNAL]   = "External",
};

static const char* BLK_TAG_NAMES[] = {
    [BLK_TAG_UNKNOWN]  = "Unknown",
    [BLK_TAG_NETWORK]  = "Network",
    [BLK_TAG_EVENT]    = "Event",
    [BLK_TAG_REGISTRY] = "Registry",
    [BLK_TAG_PLATFORM] = "Platform",
    [BLK_TAG_MEMORY]   = "Memory",
    [BLK_TAG_DATA]     = "Data",
};

static struct basic_pool block_pool;
static struct basic_pool chain_pool;

static bool tracker_initialized = false;
static MCMutex stats_mutex;

memory_chain create_chain(enum MemoryChainTag tag, const char* name, memory_chain prev) {
    i32 idx;
    mcmutex_lock(&stats_mutex);
    struct memory_chain* new_chain = basic_pool_alloc(&chain_pool, &idx);

    mcmutex_unlock(&stats_mutex);
    *new_chain = (struct memory_chain) {
        .tag        = tag,
        .prev_chain = prev,
        .next_chain = INVALID_CHAIN,
        .head       = INVALID_BLOCK,
        .tail       = INVALID_BLOCK,
        .name       = name,
    };
    if (prev >= 0) {
        struct memory_chain* prev_ptr = basic_pool_query(&chain_pool, prev);
        prev_ptr->next_chain          = idx;
    }
    return idx;
}

static void delete_block_internal(memory_block blk) {
    struct memory_block* blk_ptr = basic_pool_query(&block_pool, blk);
    platform_free(blk_ptr->start, blk_ptr->capacity);
    basic_pool_free(&block_pool, blk_ptr);
}

static void destroy_single_chain(struct memory_chain* chain_ptr) {

    memory_block blk = chain_ptr->head;
    while (blk != INVALID_BLOCK) {
        struct memory_block* block_ptr = basic_pool_query(&block_pool, blk);
        i32 next                       = block_ptr->next;
        delete_block_internal(blk);
        blk = next;
    }

    basic_pool_free(&chain_pool, chain_ptr);
}

void destroy_chain(memory_chain chain) {

    struct memory_chain* chain_ptr = basic_pool_query(&chain_pool, chain);
    mcmutex_lock(&stats_mutex);

    if (chain_ptr->prev_chain != INVALID_CHAIN) {
        struct memory_chain* prev_chain_ptr = basic_pool_query(&chain_pool, chain_ptr->prev_chain);
        prev_chain_ptr->next_chain          = INVALID_CHAIN;
    }

    memory_chain next_chain = chain_ptr->next_chain;
    while (next_chain != INVALID_CHAIN) {
        struct memory_chain* next_chain_ptr = basic_pool_query(&chain_pool, chain_ptr->next_chain);
        next_chain_ptr->prev_chain          = INVALID_CHAIN;
        next_chain                          = next_chain_ptr->next_chain;
        destroy_single_chain(next_chain_ptr);
    }

    destroy_single_chain(chain_ptr);
    // platform_abort();
    mcmutex_unlock(&stats_mutex);
}

memory_block alloc_block(u64 capacity, memory_chain chain, enum MemoryAdvice advice) {
    void* memory = platform_alloc(&capacity, advice);
    assert(memory != NULL);

    i32 index;

    mcmutex_lock(&stats_mutex);
    struct memory_block* block = basic_pool_alloc(&block_pool, &index);
    struct memory_chain* chain_ptr = basic_pool_query(&chain_pool, chain);
    mcmutex_unlock(&stats_mutex);

    *block = (struct memory_block) {
        .capacity = capacity,
        .next     = INVALID_BLOCK,
        .start    = memory,
        .type     = ALLOC_TYPE_DYNAMIC,
    };

    if (chain_ptr->tail >= 0) {
        struct memory_block* tail_ptr = basic_pool_query(&block_pool, chain_ptr->tail);
        tail_ptr->next                = index;
    } else
        chain_ptr->head = index;

    block->prev = chain_ptr->tail;
    chain_ptr->tail = index;
    chain_ptr->block_count++;

    return index;
}

void advise_block(memory_block block, enum MemoryAdvice advice) {
    struct memory_block* block_ptr = basic_pool_query(&block_pool, block);
    platform_mem_advise(block_ptr->start, block_ptr->capacity, advice);
}

void delete_block(memory_block blk) {
    mcmutex_lock(&stats_mutex);
    delete_block_internal(blk);
    mcmutex_unlock(&stats_mutex);
}

memory_block chain_head(memory_chain chain) {
    if (chain < 0)
        return INVALID_BLOCK;

    const struct memory_chain* chain_ptr = basic_pool_query(&chain_pool, chain);
    if (chain_ptr == NULL)
        return INVALID_BLOCK;

    return chain_ptr->head;
}

memory_block block_next(memory_block block) {
    if (block < 0)
        return INVALID_BLOCK;

    const struct memory_block* block_ptr = basic_pool_query(&block_pool, block);
    if (block_ptr == NULL)
        return INVALID_BLOCK;

    return block_ptr->next;
}
memory_block block_prev(memory_block block) {
    if (block < 0)
        return INVALID_BLOCK;

    const struct memory_block* block_ptr = basic_pool_query(&block_pool, block);
    if (block_ptr == NULL)
        return INVALID_BLOCK;

    return block_ptr->prev;
}

void* block_memory(memory_block block) {
    if (block < 0)
        return NULL;

    const struct memory_block* block_ptr = basic_pool_query(&block_pool, block);
    if (block_ptr == NULL)
        return NULL;

    return block_ptr->start;
}

u64 block_capacity(memory_block block) {
    if (block < 0)
        return 0ull;

    const struct memory_block* block_ptr = basic_pool_query(&block_pool, block);
    if (block_ptr == NULL)
        return 0ull;

    return block_ptr->capacity;
}

u64 block_used(memory_block block) {
    if (block < 0)
        return 0ull;

    const struct memory_block* block_ptr = basic_pool_query(&block_pool, block);
    if (block_ptr == NULL)
        return 0ull;

    return block_ptr->used;
}

void block_set_used(memory_block block, u64 value) {
    if (block < 0)
        return;

    struct memory_block* block_ptr = basic_pool_query(&block_pool, block);
    if (block_ptr != NULL)
        block_ptr->used = value;
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

void memory_init(void) {
    if (tracker_initialized)
        return;
    tracker_initialized = true;
    mcmutex_create(&stats_mutex);

    basic_pool_init(&chain_pool, 8, POOL_CHAIN);
    basic_pool_init(&block_pool, 64, POOL_BLOCK);
    // pool_init_static(&allocation_pool, 512, sizeof(memory_block), &allocation_chain);
}

static void check_leaks(void) {
    if (block_pool.size == 0) {
        log_debug("No memory leaks !");
        return;
    }
    log_error("MEMORY LEAKS DETECTED ! Here is a dump of memory statistics for you !");
    memory_dump_stats();
}

void memory_cleanup(void) {
    check_leaks();
    tracker_initialized = false;
    basic_pool_cleanup(&chain_pool);
    basic_pool_cleanup(&block_pool);

    mcmutex_destroy(&stats_mutex);
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
    return true;

    for (u64 i = 0; i < current_allocs->size; i++) {
        struct alloc_track* tmp_alloc = vect_ref(current_allocs, i);

        if (new_alloc->start >= tmp_alloc->start && new_alloc->start < tmp_alloc->end)
            return false;

        if (tmp_alloc->start >= new_alloc->start && tmp_alloc->start < new_alloc->end)
            return false;
    }
    return true;

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
    u64 total_used;
};

static void print_usage_bar(u64 used, u64 capacity, i32 bar_width) {
    i32 filled = (i32) (used * bar_width) / capacity;
    printf("[");
    if (used >= capacity || used == 0) {
        printf(used == 0 ? ANSI_BLACK : ANSI_BLUE);
        for (i32 i = 0; i < bar_width; i++) {
            printf("━");
        }
    } else {
        printf(ANSI_BLUE);
        i32 i;
        for (i = 0; i < filled; i++) {
            printf("━");
        }
        if(i - 1 >= 0) {
            printf("╸");
            i++;
        }
        if(i < bar_width) {
            printf(ANSI_BLACK "╺");
            i++;
        }
        for (;i < bar_width; i++) {
            printf("━");
        }
    }
    printf(ANSI_RESET "] ");
}

static void print_size(u64 bytes) {
    if (bytes < 1024) {
        printf(ANSI_YELLOW "%zu" ANSI_RESET "B", bytes);
    } else if (bytes < 1024 * 1024) {
        printf(ANSI_YELLOW "%.1f" ANSI_RESET "KB", bytes / 1024.0);
    } else {
        printf(ANSI_YELLOW "%.1f" ANSI_RESET "MB", bytes / (1024.0 * 1024.0));
    }
}

static void
dump_block_stats(const struct memory_block* block, memory_block idx, struct stat_dump_data* data) {
    UNUSED(data);
    printf("  Block " ANSI_MAGENTA "%-5i" ANSI_RESET ": ", idx);
    print_usage_bar(block->used, block->capacity, 30);
    printf("%5.1f%% (", (100.0 * block->used) / block->capacity);
    print_size(block->used);
    printf(" / ");
    print_size(block->capacity);
    printf(")\n");
}

static void dump_chain_stats(union basic_pool_elem elem, i32 idx, void* user_data) {
    struct stat_dump_data* data = user_data;
    struct memory_chain* track  = elem.chain;

    string tag_name = get_blk_tag_name(track->tag);

    printf("Chain " ANSI_MAGENTA "%i" ANSI_RESET ", " ANSI_YELLOW "%u" ANSI_RESET
           " blocks, tagged " ANSI_BLUE "%s" ANSI_RESET " " ANSI_CYAN "%s" ANSI_RESET ":\n",
           idx,
           track->block_count,
           cstr(&tag_name),
           elem.chain->name);

    memory_block block = track->head;
    struct memory_block* block_ptr;
    u64 chain_allocated = 0;
    u64 chain_used      = 0;
    while (block != INVALID_BLOCK) {
        block_ptr = basic_pool_query(&block_pool, block);
        chain_allocated += block_ptr->capacity;
        chain_used += block_ptr->used;
        dump_block_stats(block_ptr, block, data);
        block = block_ptr->next;
    }

    printf("  => Chain total: %.1f%% (", (100.0 * chain_used) / chain_allocated);
    print_size(chain_used);
    printf(" / ");
    print_size(chain_allocated);
    puts(")");
    data->total_allocated += chain_allocated;
    data->total_used += chain_used;
}

void memory_dump_stats(void) {
    log_info("=== MEMORY STATISTICS ===");

    struct stat_dump_data data = {0};
    printf("Total number of blocks: " ANSI_YELLOW "%u" ANSI_RESET "\n", block_pool.size);
    printf("Total number of chains: " ANSI_YELLOW "%u" ANSI_RESET "\n", chain_pool.size);
    basic_pool_foreach(&chain_pool, &dump_chain_stats, &data);

    printf("\nTotal: %.1f%% (", 100.0 * data.total_used / data.total_allocated);
    print_size(data.total_used);
    printf(" / ");
    print_size(data.total_allocated);
    puts(")");
}
