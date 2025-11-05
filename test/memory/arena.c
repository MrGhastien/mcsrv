#include "memory/allocators/arena.h"
#include "memory/_memory_internal.h"
#include "memory/mem_tags.h"
#include "memory/memory.h"
#include <unity.h>

void setUp(void) {
    memory_init();
}

void tearDown(void) {
    memory_cleanup();
}

void test_init(void) {
    Arena arena = arena_create(512, BLK_TAG_MEMORY, INVALID_CHAIN);

    arena_free(&arena, 74638);
    arena_free_ptr(&arena, (void*)0x7fff782378);

    TEST_ASSERT_EQUAL_UINT64(0, arena.length);

    arena_clear(&arena);
    TEST_ASSERT_EQUAL_UINT64(0, arena.length);
    TEST_ASSERT_EQUAL_UINT64(0, block_used(arena.current));

    arena_destroy(&arena);
}

void test_simple_alloc(void) {
    Arena arena = arena_create(512, BLK_TAG_MEMORY, INVALID_CHAIN);

    int* integers;
    u64 integers_size = sizeof *integers * 8;
    arena_allocate(&arena, integers_size);
    TEST_ASSERT_EQUAL_UINT64(integers_size, arena.length);
    TEST_ASSERT_EQUAL_UINT64(integers_size, block_used(arena.current));

    arena_free(&arena, 74638);
    TEST_ASSERT_EQUAL_UINT64(0, arena.length);
    TEST_ASSERT_EQUAL_UINT64(0, block_used(arena.current));

    integers = arena_allocate(&arena, integers_size);
    TEST_ASSERT_EQUAL_UINT64(integers_size, arena.length);
    arena_free_ptr(&arena, integers);
    TEST_ASSERT_EQUAL_UINT64(0, arena.length);
    TEST_ASSERT_EQUAL_UINT64(0, block_used(arena.current));

    arena_clear(&arena);
    TEST_ASSERT_EQUAL_UINT64(0, arena.length);
    TEST_ASSERT_EQUAL_UINT64(0, block_used(arena.current));

    arena_destroy(&arena);
}

void test_multiple_alloc(void) {
    Arena arena = arena_create(512, BLK_TAG_MEMORY, INVALID_CHAIN);

    int* integers;
    double* doubles;
    u64 integers_size = sizeof *integers * 8;
    u64 doubles_size = sizeof *doubles * 8;
    arena_allocate(&arena, integers_size);
    TEST_ASSERT_EQUAL_UINT64(integers_size, arena.length);
    TEST_ASSERT_EQUAL_UINT64(integers_size, block_used(arena.current));
    arena_allocate(&arena, doubles_size);
    TEST_ASSERT_EQUAL_UINT64(integers_size + doubles_size, arena.length);
    TEST_ASSERT_EQUAL_UINT64(integers_size + doubles_size, block_used(arena.current));

    arena_free(&arena, doubles_size);
    TEST_ASSERT_EQUAL_UINT64(integers_size, arena.length);
    TEST_ASSERT_EQUAL_UINT64(integers_size, block_used(arena.current));

    doubles = arena_allocate(&arena, doubles_size);
    TEST_ASSERT_EQUAL_UINT64(integers_size + doubles_size, arena.length);
    TEST_ASSERT_EQUAL_UINT64(integers_size + doubles_size, block_used(arena.current));

    arena_free_ptr(&arena, doubles);
    TEST_ASSERT_EQUAL_UINT64(integers_size, arena.length);
    TEST_ASSERT_EQUAL_UINT64(integers_size, block_used(arena.current));

    doubles = arena_allocate(&arena, doubles_size);
    TEST_ASSERT_EQUAL_UINT64(integers_size + doubles_size, arena.length);
    TEST_ASSERT_EQUAL_UINT64(integers_size + doubles_size, block_used(arena.current));

    arena_clear(&arena);
    TEST_ASSERT_EQUAL_UINT64(0, arena.length);
    TEST_ASSERT_EQUAL_UINT64(0, block_used(arena.current));

    arena_destroy(&arena);
}

void test_resize(void) {
    Arena arena = arena_create(4096, BLK_TAG_MEMORY, INVALID_CHAIN);

    int* integers;
    double* doubles;
    u64 integers_size = sizeof *integers * 1024;
    u64 doubles_size = sizeof *doubles * 4096;
    arena_allocate(&arena, integers_size);
    TEST_ASSERT_EQUAL_UINT64(integers_size, arena.length);
    memory_block prev_blk = arena.current;
    TEST_ASSERT_EQUAL_UINT64(integers_size, block_used(arena.current));
    arena_allocate(&arena, doubles_size);
    TEST_ASSERT_EQUAL_UINT64(integers_size + doubles_size, arena.length);
    TEST_ASSERT_EQUAL_UINT64(doubles_size, block_used(arena.current));
    TEST_ASSERT_NOT_EQUAL_INT32(prev_blk, arena.current);

    arena_free(&arena, doubles_size);
    TEST_ASSERT_EQUAL_UINT64(integers_size, arena.length);
    TEST_ASSERT_EQUAL_UINT64(integers_size, block_used(arena.current));

    doubles = arena_allocate(&arena, doubles_size);
    TEST_ASSERT_EQUAL_UINT64(integers_size + doubles_size, arena.length);

    arena_free_ptr(&arena, doubles);
    TEST_ASSERT_EQUAL_UINT64(integers_size, arena.length);
    TEST_ASSERT_EQUAL_UINT64(integers_size, block_used(arena.current));

    doubles = arena_allocate(&arena, doubles_size);
    TEST_ASSERT_EQUAL_UINT64(integers_size + doubles_size, arena.length);
    TEST_ASSERT_EQUAL_UINT64(integers_size + doubles_size, block_used(arena.current));

    arena_clear(&arena);
    TEST_ASSERT_EQUAL_UINT64(0, arena.length);
    TEST_ASSERT_EQUAL_UINT64(0, block_used(arena.current));

    arena_destroy(&arena);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_init);
    RUN_TEST(test_simple_alloc);
    RUN_TEST(test_multiple_alloc);
    RUN_TEST(test_resize);

    return UNITY_END();
}
