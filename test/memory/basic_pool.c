#include "memory/stats/basic_pool.h"
#include "memory/memory.h"
#include "memory/_memory_internal.h"
#include <unity.h>

static struct basic_pool pool;

void setUp(void) {
    basic_pool_init(&pool, 64, POOL_BLOCK);
}

void tearDown(void) {
    basic_pool_cleanup(&pool);
}

void test_basic_pool_create(void) {

    TEST_ASSERT_EQUAL_INT32(pool.capacity, 64);
    TEST_ASSERT_EQUAL_INT32(pool.size, 0);
    TEST_ASSERT_EQUAL_PTR(pool.type, POOL_BLOCK);
    TEST_ASSERT_EQUAL_PTR(pool.head, pool.blocks);
}

void test_basic_pool_alloc(void) {

    i32 idx;
    struct memory_block* block = basic_pool_alloc(&pool, &idx);
    UNUSED(block);

    TEST_ASSERT_EQUAL_INT32(1, pool.size);

    basic_pool_free(&pool, block);

    TEST_ASSERT_EQUAL_INT32(0, pool.size);
}

void test_basic_pool_full_alloc(void) {
    for(i32 i = 0; i < 64; i++) {
        basic_pool_alloc(&pool, NULL);
    }

    TEST_ASSERT_EQUAL_INT32(64, pool.size);
    TEST_ASSERT_EQUAL_INT32(64, pool.capacity);

    struct memory_block* extra_block = basic_pool_alloc(&pool, NULL);
    TEST_ASSERT_EQUAL_INT32(65, pool.size);
    TEST_ASSERT_EQUAL_INT32(128, pool.capacity);

    basic_pool_free(&pool, extra_block);
    TEST_ASSERT_EQUAL_INT32(64, pool.size);
    TEST_ASSERT_EQUAL_INT32(128, pool.capacity);
}

void test_basic_pool_query(void) {
    i32 idx1, idx2;
    struct memory_block* alloc_block_1 = basic_pool_alloc(&pool, &idx1);
    struct memory_block* alloc_block_2 = basic_pool_alloc(&pool, &idx2);

    struct memory_block* query_block_1 = basic_pool_query(&pool, idx1);
    struct memory_block* query_block_2 = basic_pool_query(&pool, idx2);

    TEST_ASSERT_EQUAL_INT(2, pool.size);

    TEST_ASSERT_EQUAL_PTR(alloc_block_1, query_block_1);
    TEST_ASSERT_EQUAL_PTR(alloc_block_2, query_block_2);

    struct memory_block* query_block_random = basic_pool_query(&pool, 28384);
    TEST_ASSERT_EQUAL_PTR(NULL, query_block_random);
}

void test_basic_pool_mixed(void) {
    struct memory_block* blocks[64];
    i32 indices[64];

    for(i32 i = 0; i < 47; i++) {
        blocks[i] = basic_pool_alloc(&pool, &indices[i]);
    }

    for(i32 i = 0; i < 47; i += 2) {
        basic_pool_free(&pool, blocks[i]);
    }
    TEST_ASSERT_EQUAL_INT(47 - 24, pool.size);

    TEST_ASSERT_EQUAL_PTR(NULL, basic_pool_query(&pool, indices[2]));

}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_basic_pool_create);
    RUN_TEST(test_basic_pool_alloc);
    RUN_TEST(test_basic_pool_full_alloc);
    RUN_TEST(test_basic_pool_query);
    RUN_TEST(test_basic_pool_mixed);

    return UNITY_END();
}
