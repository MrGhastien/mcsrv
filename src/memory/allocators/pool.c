#include "pool.h"
#include "logger.h"
#include "memory/_memory_internal.h"
#include "memory/mem_tags.h"
#include "platform/mem_advice.h"
#include "utils/bitwise.h"
#include "utils/math.h"

#include <stdlib.h>

#define NODE_HEADER_SIZE (sizeof(struct obj_node) - sizeof(struct obj_node*))

struct obj_node {
    bool allocated;
    memory_block blk;
    struct obj_node* next;
};

static struct obj_node* get_node_from_idx(PoolAllocator* pool, i64 index) {
    memory_block blk = chain_head(pool->mem);
    i64 total_stride = pool->stride + NODE_HEADER_SIZE;

    while (blk != INVALID_BLOCK) {
        i64 blk_capacity = block_capacity(blk) / total_stride;

        if (index < blk_capacity) {
            //*out_block = blk;
            return offset(block_memory(blk), index * total_stride);
        }
        index -= blk_capacity;
        blk = block_next(blk);
    }
    return NULL;
}

static void add_node_to_free_list(PoolAllocator* pool, struct obj_node* node) {
    node->next      = NULL;
    node->allocated = FALSE;
    if (!pool->head)
        pool->head = node;
    if (pool->tail)
        pool->tail->next = node;
    pool->tail = node;
    pool->size--;
}

static void prepare_block(PoolAllocator* pool, const memory_block block) {
    u64 total_stride           = pool->stride + NODE_HEADER_SIZE;
    u64 block_element_capacity = block_capacity(block) / total_stride;
    for (u64 i = 0; i < block_element_capacity; i++) {
        struct obj_node* node = offset(block_memory(block), i * total_stride);
        add_node_to_free_list(pool, node);
        node->blk = block;
    }
    block_set_used(block, NODE_HEADER_SIZE * block_element_capacity);
}

static void init_free_list(PoolAllocator* pool) {
    memory_block blk = chain_head(pool->mem);
    if (blk == INVALID_BLOCK)
        return;
    // pool->head = block_memory(blk);
    do {
        prepare_block(pool, blk);
        blk = block_next(blk);
    } while (blk != INVALID_BLOCK);
    pool->size = 0;
}

static void pool_init_common(PoolAllocator* pool,
                             u32 capacity,
                             u32 stride,
                             enum MemoryChainTag tag,
                             const char* name,
                             memory_chain prev) {
    u64 actual_stride = max_u64(stride, sizeof(struct obj_node*));
    pool->mem         = create_chain(tag, name, prev);
    pool->capacity    = capacity;
    pool->stride      = actual_stride;
    pool->head        = NULL;
    pool->tail        = NULL;
    pool->size        = 0;

    u64 total_stride = actual_stride + NODE_HEADER_SIZE;
    memory_block blk = alloc_block(capacity * total_stride, pool->mem, MEM_ADVICE_RANDOM);
    pool->capacity   = block_capacity(blk) / total_stride;
    init_free_list(pool);
}

void _pool_init(PoolAllocator* pool,
                u32 capacity,
                u32 stride,
                enum MemoryChainTag tag,
                const char* name,
                memory_chain prev) {
    pool_init_common(pool, capacity, stride, tag, name, prev);
    pool->dynamic = FALSE;
}

void _pool_init_dynamic(PoolAllocator* pool,
                        u32 initial_capacity,
                        u32 stride,
                        enum MemoryChainTag tag,
                        const char* name,
                        memory_chain prev) {
    pool_init_common(pool, initial_capacity, stride, tag, name, prev);
    pool->dynamic = TRUE;
}

void pool_init_static(PoolAllocator* pool, u32 capacity, u32 stride, memory_chain chain) {
    pool->mem      = chain;
    pool->capacity = capacity;
    pool->dynamic  = FALSE;
    pool->stride   = stride;
    init_free_list(pool);
}

void pool_destroy(PoolAllocator* pool) {
    destroy_chain(pool->mem);
    *pool = (PoolAllocator) {
        .mem     = INVALID_CHAIN,
        .head    = NULL,
        .tail    = NULL,
        .dynamic = FALSE,
    };
}

static bool ensure_capacity(PoolAllocator* pool, u64 size) {
    if (size <= pool->capacity)
        return TRUE;

    if (!pool_is_dynamic(pool)) {
        log_error("Cannot resize a static pool allocator !");
        return FALSE;
    }

    memory_block blk = alloc_block(
        (pool->capacity >> 1) * (pool->stride + NODE_HEADER_SIZE), pool->mem, MEM_ADVICE_RANDOM);
    if (blk == INVALID_BLOCK)
        abort();

    prepare_block(pool, blk);

    pool->capacity += block_capacity(blk);
    return TRUE;
}

void pool_clear(PoolAllocator* pool) {
    u64 size         = pool_size(pool);
    u64 total_stride = pool->stride + NODE_HEADER_SIZE;
    u64 count        = 0;
    memory_block blk = chain_head(pool->mem);
    while (blk != INVALID_BLOCK && count < size) {
        for (u64 i = 0; i < block_capacity(blk) && count < size; i += total_stride) {
            struct obj_node* node = offset(block_memory(blk), i);
            if (node->allocated) {
                add_node_to_free_list(pool, node);
                count++;
            }
        }
        advise_block(blk, MEM_ADVICE_UNUSED);
    }
    pool->size = 0;
}

void* pool_alloc(PoolAllocator* pool, i64* out_index) {
    if (!ensure_capacity(pool, pool->size + 1))
        abort();

    struct obj_node* node = pool->head;
    void* ptr             = offset(node, NODE_HEADER_SIZE);
    pool->head            = pool->head->next;

    if (out_index) {
        i64 total        = 0;
        memory_block blk = chain_head(pool->mem);
        do {
            if (is_addr_in_block(ptr, blk)) {
                total += ptr_diff(node, block_memory(blk));
                break;
            }
            total += block_capacity(blk);
            blk = block_next(blk);
        } while (blk != INVALID_BLOCK);
        *out_index = total / (pool->stride + NODE_HEADER_SIZE);
    }
    block_set_used(node->blk, block_used(node->blk) + pool->stride);
    pool->size++;
    node->allocated = TRUE;
    node->next      = NULL;
    return ptr;
}

static bool free_node(PoolAllocator* pool, struct obj_node* node) {
    if (!node->allocated)
        return FALSE;

    block_set_used(node->blk, block_used(node->blk) - pool->stride);

    add_node_to_free_list(pool, node);
    return TRUE;
}

bool pool_free(PoolAllocator* pool, void* ptr) {
    // TODO: Check if address is valid

    return free_node(pool, offset(ptr, -NODE_HEADER_SIZE));
}

bool pool_free_idx(PoolAllocator* pool, i64 idx) {
    if (idx < 0 || idx >= pool->capacity)
        return FALSE;

    struct obj_node* node = get_node_from_idx(pool, idx);
    if (!node->allocated)
        return FALSE;

    return free_node(pool, node);
}

void* pool_get(PoolAllocator* pool, i64 index) {
    if (index < 0 || index >= pool->capacity)
        return NULL;

    struct obj_node* node = get_node_from_idx(pool, index);
    if (!node->allocated)
        return NULL;
    return offset(node, NODE_HEADER_SIZE);
}

void pool_foreach(const PoolAllocator* pool, void (*action)(void*, i64, void*), void* user_data) {
    u64 size         = pool_size(pool);
    u64 total_stride = pool->stride + NODE_HEADER_SIZE;

    u64 idx   = 0;
    u64 count = 0;

    memory_block blk = chain_head(pool->mem);
    while (blk != INVALID_BLOCK && count < size) {
        for (u64 i = 0; i < block_capacity(blk) && count < size; i += total_stride) {
            struct obj_node* node = offset(block_memory(blk), i);
            if (node->allocated) {
                action(offset(node, NODE_HEADER_SIZE), idx, user_data);
                count++;
            }
            idx++;
        }
    }
}
