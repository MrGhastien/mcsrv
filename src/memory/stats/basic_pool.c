#include "basic_pool.h"
#include "platform/platform.h"
#include "utils/bitwise.h"
#include "utils/debug.h"
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
        mc_abort();
    }
}

void basic_pool_init(struct basic_pool* pool, u32 capacity, enum PoolNodeType type) {
    pool->size = 0;
    pool->head = pool->blocks;
    pool->tail = pool->blocks + (pool->capacity - 1);
    pool->type = type;

    u64 cap64 = capacity * sizeof(struct node);
    pool->blocks = platform_alloc(&cap64);
    pool->capacity = cap64 / sizeof(struct node);

    for (i64 i = 0; i < pool->capacity; i++) {
        struct node* n = &pool->blocks[i];
        n->data.next = i + 1;
        n->allocated = FALSE;
    }
    pool->blocks[pool->capacity - 1].data.next = -1;
}

void basic_pool_cleanup(struct basic_pool* pool) {
    platform_free(pool->blocks, pool->capacity * sizeof(struct node));
}

static void grow(struct basic_pool* pool) {
    u64 stride = sizeof(struct node);
    u64 cap64 = (pool->capacity << 1ULL) * stride;
    struct node* new_array = platform_alloc(&cap64);
    if (!new_array)
        mc_abort();

    memcpy(new_array, pool->blocks, pool->capacity * stride);

    u32 new_cap = cap64 / stride;
    for (i64 i = pool->capacity; i < new_cap - 1; i++) {
        struct node* n = &pool->blocks[i];
        n->data.next = i + 1;
        n->allocated = FALSE;
    }
    pool->head = &new_array[pool->head - pool->blocks];
    pool->tail = &new_array[pool->tail - pool->blocks];
    pool->blocks[new_cap - 1].data.next = -1;
    pool->tail->data.next = pool->capacity;
    pool->capacity = new_cap;
    platform_free(pool->blocks, pool->capacity * stride);
    pool->blocks = new_array;
}

void* basic_pool_alloc(struct basic_pool* pool, i32* out_idx) {
    if (pool->size == pool->capacity) {
        grow(pool);
    }

    struct node* target = pool->head;
    i64 next = target->data.next;
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
        mc_abort();
    }
    if(out_idx)
        *out_idx = 0;

    return &target->data.block;
}

void basic_pool_free(struct basic_pool* pool, void* ptr) {
    struct node* node = offset(ptr, -sizeof(bool));
    i64 index = (u64) node - (u64) pool->blocks;

    node->allocated = FALSE;
    node->data.next = -1;
    if (pool->tail)
        pool->tail->data.next = index;
    else
        pool->head = node;
    pool->tail = node;
}

void* basic_pool_query(struct basic_pool* pool, i32 idx) {
    return &pool->blocks[idx].data.block;
}

void basic_pool_foreach(struct basic_pool* pool, action action, void* user_data) {
    for (u32 i = 0; i < pool->capacity; i++) {
        struct node* node = &pool->blocks[i];
        if(node->allocated)
            // Here the `block` and `chain` pointers are stored in the same memory.
            // Setting either of them sets the other one.
            action((union basic_pool_elem) {.block = &node->data.block}, i, user_data);
    }
}
