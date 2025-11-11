#include "basic_pool.h"
#include "logger.h"
#include "platform/mem_advice.h"
#include "platform/platform.h"
#include "utils/bitwise.h"
#include <string.h>

struct node {
    bool allocated;
    union {
        i64 next;
        struct memory_block block;
        struct memory_chain chain;
    } data;
};

/*
static i64 node_index(struct basic_pool* pool, struct node* node) {
    return node - pool->blocks;
}
*/

static inline u64 total_stride(const struct basic_pool* pool) {
    switch (pool->type) {
    case POOL_BLOCK:
        return sizeof(struct memory_block) + sizeof(bool);
    case POOL_CHAIN:
        return sizeof(struct memory_chain) + sizeof(bool);
    default:
        platform_abort();
        return 0;
    }
}

void basic_pool_init(struct basic_pool* pool, u32 capacity, enum PoolNodeType type) {
    pool->size = 0;
    pool->type = type;

    u64 cap64    = capacity * sizeof(struct node);
    pool->blocks = platform_alloc(&cap64, MEM_ADVICE_RANDOM);
    cap64 /= sizeof(struct node);

    for (u64 i = 0; i < cap64; i++) {
        struct node* n = &pool->blocks[i];
        n->data.next   = i + 1;
        n->allocated   = FALSE;
    }
    pool->head                        = &pool->blocks[0];
    pool->tail                        = &pool->blocks[cap64 - 1];
    pool->blocks[cap64 - 1].data.next = -1;
    pool->capacity                    = cap64;
}

void basic_pool_cleanup(struct basic_pool* pool) {
    platform_free(pool->blocks, pool->capacity * sizeof(struct node));

    *pool = (struct basic_pool) {0};
}

static void grow(struct basic_pool* pool) {
    u64 stride             = sizeof(struct node);
    u64 cap64              = (pool->capacity << 1ULL) * stride;
    struct node* new_array = platform_alloc(&cap64, MEM_ADVICE_RANDOM);
    if (!new_array)
        platform_abort();

    memcpy(new_array, pool->blocks, pool->capacity * stride);

    u32 new_cap = cap64 / stride;
    for (i64 i = pool->capacity; i < new_cap - 1; i++) {
        struct node* n = &new_array[i];
        n->data.next   = i + 1;
        n->allocated   = FALSE;
    }
    new_array[new_cap - 1].data.next = -1;

    pool->head            = &new_array[pool->capacity];
    pool->tail            = &new_array[new_cap - 1];
    pool->tail->data.next = pool->capacity;
    platform_free(pool->blocks, pool->capacity * stride);
    pool->capacity = new_cap;
    pool->blocks   = new_array;
}

void* basic_pool_alloc(struct basic_pool* pool, i32* out_idx) {
    if (pool->size == pool->capacity) {
        grow(pool);
    }

    struct node* target = pool->head;
    i64 next            = target->data.next;
    if (next < 0) {
        pool->head = NULL;
        pool->tail = NULL;
    } else
        pool->head = &pool->blocks[next];

    target->allocated = TRUE;
    switch (pool->type) {
    case POOL_BLOCK:
        target->data.block = (struct memory_block) {0};
        break;
    case POOL_CHAIN:
        target->data.chain = (struct memory_chain) {0};
        break;
    default:
        platform_abort();
    }
    if (out_idx) {
        i32 idx = target - pool->blocks;
        platform_assert((u32) idx < pool->capacity && idx >= 0,
                        "Index of newly allocated node is out of bounds !");
        *out_idx = idx;
    }

    pool->size++;

    return &target->data.block;
}

void basic_pool_free(struct basic_pool* pool, void* ptr) {
    struct node* node = offset(ptr, -offsetof(struct node, data));
    i64 index         = node - pool->blocks;

    platform_assert(index < pool->capacity && index >= 0,
                    "Index of node to free is out of bounds !");

    node->allocated = FALSE;
    node->data.next = -1;
    if (pool->tail)
        pool->tail->data.next = index;
    else
        pool->head = node;
    pool->tail = node;

    pool->size--;
}

void* basic_pool_query(struct basic_pool* pool, i32 idx) {
    if (idx < 0 || (u32) idx >= pool->capacity)
        return NULL;
    if (!pool->blocks[idx].allocated)
        return NULL;
    return &pool->blocks[idx].data;
}

void basic_pool_foreach(struct basic_pool* pool, action action, void* user_data) {
    for (u32 i = 0; i < pool->capacity; i++) {
        struct node* node = &pool->blocks[i];
        if (node->allocated)
            // Here the `block` and `chain` pointers are stored in the same memory.
            // Setting either of them sets the other one.
            action((union basic_pool_elem) {.block = &node->data.block}, i, user_data);
    }
}
