#ifndef BASIC_POOL_H
#define BASIC_POOL_H

#include "definitions.h"
#include "../_memory_internal.h"

enum PoolNodeType {
    POOL_BLOCK,
    POOL_CHAIN,
    POOL_ALLOC,
};

struct basic_pool {
    struct node* blocks;
    struct node* head;
    struct node* tail;
    enum PoolNodeType type;
    u32 capacity;
    u32 size;
};

union basic_pool_elem {
    struct memory_block* block;
    struct memory_chain* chain;
};
typedef void(*action)(union basic_pool_elem elem, i32 idx, void* user_data);

void basic_pool_init(struct basic_pool* pool, u32 capacity, enum PoolNodeType type);
void basic_pool_cleanup(struct basic_pool* pool);

void* basic_pool_alloc(struct basic_pool* pool, i32* out_idx);
void basic_pool_free(struct basic_pool* pool, void* block);

void* basic_pool_query(struct basic_pool* pool, i32 idx);
void basic_pool_foreach(struct basic_pool* pool, action action, void* user_data);

#endif /* ! BASIC_POOL_H */
