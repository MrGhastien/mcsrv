#include "memory/allocators/arena.h"
#include "logger.h"
#include "memory/_memory_internal.h"
#include "memory/mem_tags.h"
#include "memory/memory.h"
#include "unity_internals.h"
#include "utils/bitwise.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unity.h>

void setUp(void) {
    memory_init();
}

void tearDown(void) {
    memory_cleanup();
}

typedef struct {
    uint64_t s;
} Rng;

static uint64_t rng_next(Rng* r) {
    uint64_t z = (r->s += 0x9E3779B97F4A7C15ull);
    z          = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z          = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}
static size_t rng_range(Rng* r, size_t n) {
    return (size_t) (rng_next(r) % n);
}

typedef struct {
    uint8_t* memory;
    size_t size;
    uint8_t tag;
} Alloc; // size arrondie
typedef struct {
    ArenaCheckpoint t;
    size_t n_allocs;
    size_t used;
} Checkpoint;

#define MAX_ALLOCS 65536
#define MAX_CHECKPOINTS 512
typedef struct {
    Alloc allocs[MAX_ALLOCS];
    size_t alloc_count;
    Checkpoint checkpoints[MAX_CHECKPOINTS];
    size_t checkpoint_count;
} Model;

static size_t round_up(size_t n) {
    return (n + sizeof(uintptr_t) - 1) & ~(sizeof(uintptr_t) - 1);
}

static size_t model_used(const Model* md) {
    size_t s = 0;
    for (size_t i = 0; i < md->alloc_count; i++)
        s += md->allocs[i].size;
    return s;
}

#define ERRSZ 256
#define CHECK(c, ...)                                                                              \
    do {                                                                                           \
        if (!(c)) {                                                                                \
            int k = snprintf(err, ERRSZ, "step %zu: ", step);                                      \
            snprintf(err + k, ERRSZ - k, __VA_ARGS__);                                             \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

static bool verify(Arena* a, const Model* md, char* err, size_t step) {
    UNUSED(a);
    for (size_t i = 0; i < md->alloc_count; i++) {
        const Alloc* al = &md->allocs[i];
        CHECK(((uintptr_t) al->memory & (sizeof(uintptr_t) - 1)) == 0, "alloc %zu mal alignée", i);
        for (size_t j = 0; j < al->size; j++)
            CHECK(al->memory[j] == al->tag, "alloc %zu corrompue à l'octet %zu", i, j);
    }
    /* ArenaStats s = arena_stats(a); */
    /* CHECK(s.used == model_used(md), "used=%zu, attendu %zu", s.used, model_used(md)); */
    /* CHECK(s.used + s.wasted <= s.reserved, "stats incohérentes"); */
    /* CHECK(arena_check(a), "arena_check a échoué"); */
    return true;
}
static size_t pick_size(Rng* r) {
    size_t k = rng_range(r, 100);
    if (k < 70)
        return 1 + rng_range(r, 64); // petites
    if (k < 95)
        return 1 + rng_range(r, 2000); // moyennes, franchissent les blocs
    return 1 + rng_range(r, 20000);    // plus grosses qu'un bloc minimal
}

static bool run_inner(Arena* a, uint64_t seed, size_t nops, char* err) {
    Rng r            = {seed};
    Model model      = {0};
    uint8_t next_tag = 1;

    for (size_t step = 0; step < nops; step++) {
        size_t scope_base =
            model.checkpoint_count ? model.checkpoints[model.checkpoint_count - 1].n_allocs : 0;

        switch (rng_range(&r, 10)) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5: { // alloc
            if (model.alloc_count == MAX_ALLOCS)
                break;
            size_t alloc_size  = pick_size(&r);
            uint8_t* allocated = arena_allocate(a, alloc_size);
            CHECK(allocated != NULL, "alloc(%zu) a renvoyé NULL", alloc_size);
            size_t padded_size = round_up(alloc_size);
            for (size_t i = 0; i < model.alloc_count; i++) { // disjonction
                const Alloc* alloc = &model.allocs[i];
                CHECK(allocated + padded_size <= alloc->memory ||
                          alloc->memory + alloc->size <= allocated,
                      "chevauchement avec %zu",
                      i);
            }
            uint8_t tag = next_tag++;
            if (!next_tag)
                next_tag = 1;
            memset(allocated, tag, padded_size);
            model.allocs[model.alloc_count++] = (Alloc){allocated, padded_size, tag};
        } break;
        case 6: // temp_begin
            if (model.checkpoint_count == MAX_CHECKPOINTS)
                break;
            model.checkpoints[model.checkpoint_count++] =
                (Checkpoint){arena_create_checkpoint(a), model.alloc_count, model_used(&model)};
            break;
        case 7: { // temp_end
            if (!model.checkpoint_count)
                break;
            Checkpoint mk = model.checkpoints[--model.checkpoint_count];
            arena_restore_checkpoint(mk.t);
            model.alloc_count = mk.n_allocs;
        } break;
        case 8: // free(n), dernière alloc
            if (model.alloc_count > scope_base) {
                arena_free(a, model.allocs[model.alloc_count - 1].size);
                model.alloc_count--;
            }
            break;
        case 9: // free(ptr), alloc au hasard
            if (model.alloc_count > scope_base) {
                size_t i = scope_base + rng_range(&r, model.alloc_count - scope_base);
                // CHECK(arena_free_ptr(a, md.allocs[i].p), "free(ptr) refusé pour alloc %zu", i);
                arena_free_ptr(a, model.allocs[i].memory);
                model.alloc_count = i;
            }
            break;
        }
        if (!verify(a, &model, err, step))
            return false;
    }
    return true;
}

static bool run_sequence(uint64_t seed, size_t nops, char* err) {
    Arena a = arena_create(100000, BLK_TAG_UNKNOWN, INVALID_CHAIN);
    bool ok = run_inner(&a, seed, nops, err);
    arena_destroy(&a);
    return ok;
}

void test_init(void) {
    Arena arena = arena_create(512, BLK_TAG_MEMORY, INVALID_CHAIN);

    arena_free(&arena, 74638);
    printf("%zu\n", sizeof(struct memory_block));

    arena_clear(&arena);
    TEST_ASSERT_EQUAL_UINT64(0, block_used(arena.current_block));

    arena_destroy(&arena);
}

void test_simple_alloc(void) {
    Arena arena = arena_create(512, BLK_TAG_MEMORY, INVALID_CHAIN);

    int* integers;
    u64 integers_size = sizeof *integers * 8;
    arena_allocate(&arena, integers_size);
    TEST_ASSERT_EQUAL_UINT64(integers_size, ptr_diff(arena.cursor, block_memory(arena.current_block)));

    arena_free(&arena, 74638);
    TEST_ASSERT_EQUAL_UINT64(0, block_used(arena.current_block));

    integers = arena_allocate(&arena, integers_size);
    arena_free_ptr(&arena, integers);
    TEST_ASSERT_EQUAL_UINT64(0, block_used(arena.current_block));

    arena_clear(&arena);
    TEST_ASSERT_EQUAL_UINT64(0, block_used(arena.current_block));

    arena_destroy(&arena);
}

void test_multiple_alloc(void) {
    Arena arena = arena_create(512, BLK_TAG_MEMORY, INVALID_CHAIN);

    int* integers;
    double* doubles;
    u64 integers_size = sizeof *integers * 8;
    u64 doubles_size  = sizeof *doubles * 8;
    arena_allocate(&arena, integers_size);
    TEST_ASSERT_EQUAL_UINT64(integers_size, block_used(arena.current_block));
    arena_allocate(&arena, doubles_size);
    TEST_ASSERT_EQUAL_UINT64(integers_size + doubles_size, block_used(arena.current_block));

    arena_free(&arena, doubles_size);
    TEST_ASSERT_EQUAL_UINT64(integers_size, block_used(arena.current_block));

    doubles = arena_allocate(&arena, doubles_size);
    TEST_ASSERT_EQUAL_UINT64(integers_size + doubles_size, block_used(arena.current_block));

    arena_free_ptr(&arena, doubles);
    TEST_ASSERT_EQUAL_UINT64(integers_size, block_used(arena.current_block));

    doubles = arena_allocate(&arena, doubles_size);
    TEST_ASSERT_EQUAL_UINT64(integers_size + doubles_size, block_used(arena.current_block));

    arena_clear(&arena);
    TEST_ASSERT_EQUAL_UINT64(0, block_used(arena.current_block));

    arena_destroy(&arena);
}

void test_resize(void) {
    Arena arena = arena_create(4096, BLK_TAG_MEMORY, INVALID_CHAIN);

    int* integers;
    double* doubles;
    u64 integers_size = sizeof *integers * 1024;
    u64 doubles_size  = sizeof *doubles * 4096;
    arena_allocate(&arena, integers_size);
    memory_block prev_blk = arena.current_block;
    TEST_ASSERT_EQUAL_UINT64(integers_size, block_used(arena.current_block));
    arena_allocate(&arena, doubles_size);
    TEST_ASSERT_EQUAL_UINT64(doubles_size, block_used(arena.current_block));
    TEST_ASSERT_NOT_EQUAL_INT32(prev_blk, arena.current_block);

    arena_free(&arena, doubles_size);
    TEST_ASSERT_EQUAL_UINT64(integers_size, block_used(arena.current_block));

    doubles = arena_allocate(&arena, doubles_size);

    arena_free_ptr(&arena, doubles);
    TEST_ASSERT_EQUAL_UINT64(integers_size, block_used(arena.current_block));

    doubles = arena_allocate(&arena, doubles_size);
    TEST_ASSERT_EQUAL_UINT64(doubles_size, block_used(arena.current_block));

    arena_clear(&arena);
    TEST_ASSERT_EQUAL_UINT64(0, block_used(arena.current_block));

    arena_destroy(&arena);
}

void test_sequence_1(void) {
    char err[ERRSZ];
    u64 seed = 1;
    if (!run_sequence(seed, 10000, err)) {
        char msg[ERRSZ + 64];
        snprintf(msg, sizeof msg, "seed=%llu, %s", (unsigned long long) seed, err);
        TEST_FAIL_MESSAGE(msg);
    }
}

void test_sequence_15(void) {
    char err[ERRSZ];
    u64 seed = 15;
    if (!run_sequence(seed, 10000, err)) {
        char msg[ERRSZ + 64];
        snprintf(msg, sizeof msg, "seed=%llu, %s", (unsigned long long) seed, err);
        TEST_FAIL_MESSAGE(msg);
    }
}

void test_random_sequences(void) {
    char err[ERRSZ];
    for (uint64_t seed = 1; seed <= 2000; seed++) {
        log_debugf("Running random sequence seed %llu", seed);
        if (!run_sequence(seed, 10000, err)) {
            char msg[ERRSZ + 64];
            snprintf(msg, sizeof msg, "seed=%llu, %s", (unsigned long long) seed, err);
            TEST_FAIL_MESSAGE(msg);
        }
    }
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_init);
    RUN_TEST(test_simple_alloc);
    RUN_TEST(test_multiple_alloc);
    RUN_TEST(test_resize);
    RUN_TEST(test_sequence_1);
    RUN_TEST(test_sequence_15);
    RUN_TEST(test_random_sequences);

    return UNITY_END();
}
