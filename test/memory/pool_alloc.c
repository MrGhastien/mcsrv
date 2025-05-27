#include "memory/memory.h"
#include "memory/_memory_internal.h"
#include "memory/allocators/pool.h"
#include "memory/mem_tags.h"
#include "memory/stats/basic_pool.h"
#include <stdio.h>
#include <unity.h>

static PoolAllocator pool;

struct obj_node {
    bool allocated;
    struct obj_node* next;
};

void setUp(void) {
    memory_init();
}

void tearDown(void) {
    memory_cleanup();
}

static void check_free_list(void) {
    struct obj_node* node = pool.head;

    for (u32 i = 0; i < pool.capacity; i++) {
        TEST_ASSERT(node->next != NULL || i >= pool.capacity - 1);
        if(node->allocated) {
            char tmp[128];
            snprintf(tmp, 128, "Block at index %u is allocated on setup", i);
            TEST_FAIL_MESSAGE(tmp);
        }
        node = node->next;
    }
}

void test_init_static(void) {
    pool_init(&pool, 64, sizeof(i32), BLK_TAG_MEMORY, INVALID_CHAIN);

    TEST_ASSERT_EQUAL_UINT32(0, pool.size);
    TEST_ASSERT_EQUAL_UINT32(64, pool.capacity);
    TEST_ASSERT_EQUAL_UINT32(sizeof(i32), pool.stride);
    TEST_ASSERT_EQUAL_UINT32(pool.capacity, block_capacity(chain_head(pool.mem)) / (pool.stride + sizeof(struct obj_node) - sizeof(struct obj_node*)));
    TEST_ASSERT_GREATER_OR_EQUAL(sizeof(struct obj_node*), pool.stride);

    TEST_ASSERT_GREATER_OR_EQUAL(0, pool.mem);

    TEST_ASSERT_NOT_EQUAL_MESSAGE(NULL, pool.head, "Pool head is null!");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(NULL, pool.tail, "Pool tail is null!");

    check_free_list();

    void* ptr = block_memory(chain_head(pool.mem));
    TEST_ASSERT_EQUAL_PTR_MESSAGE(ptr, pool.head, "Pool head is not pointing to the first block!");

    pool_destroy(&pool);
}

void test_single_alloc(void) {
    pool_init(&pool, 64, sizeof(i32), BLK_TAG_MEMORY, INVALID_CHAIN);

    i64 idx;
    i32* ptr = pool_alloc(&pool, &idx);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, pool.size, "Pool size is not incremented when allocating");
    
    i32* ptr2 = pool_get(&pool, idx);
    TEST_ASSERT_EQUAL_PTR(ptr, ptr2);

    pool_free(&pool, ptr);
    i32* ptr3 = pool_get(&pool, idx);
    TEST_ASSERT_EQUAL_PTR(NULL, ptr3);

    pool_destroy(&pool);
}

void test_bulk_allocs(void) {
    pool_init(&pool, 64, sizeof(i32), BLK_TAG_MEMORY, INVALID_CHAIN);

    i32* pointers[64];
    i64 indices[64];

    for(i32 i = 0; i < 64; i++) {
        pointers[i] = pool_alloc(&pool, &indices[i]);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(i + 1, pool.size, "Pool size is not incremented when allocating");
        TEST_ASSERT_TRUE_MESSAGE(pool.size >= pool.capacity || pool.head != NULL, "Pool head is NULL but there are available blocks");
        TEST_ASSERT_TRUE_MESSAGE(pool.size >= pool.capacity || pool.tail != NULL, "Pool tail is NULL but there are available blocks");
    }

    UNUSED(pointers);

    pool_destroy(&pool);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_init_static);
    RUN_TEST(test_single_alloc);
    RUN_TEST(test_bulk_allocs);

    return UNITY_END();
}
