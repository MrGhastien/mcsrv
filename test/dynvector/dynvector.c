#include "containers/vector.h"
#include "logger.h"
#include "memory/arena.h"
#include <assert.h>

static void test1(void) {
    Arena arena = arena_create(1 << 20);

    DynVector vector;
    dvect_init(&vector, &arena, 4, sizeof(u64));

    dvect_add_imm(&vector, 7837, u64);
    dvect_add_imm(&vector, 1, u64);
    dvect_add_imm(&vector, 3, u64);
    dvect_add_imm(&vector, 5, u64);
    dvect_add_imm(&vector, 7, u64);
    dvect_add_imm(&vector, 19, u64);
    dvect_add_imm(&vector, 9, u64);

    u64 num;
    dvect_get(&vector, 4, &num);
    assert(num == 7);

    dvect_pop(&vector, &num);
    assert(num == 9);

    dvect_remove(&vector, 2, &num);
    assert(num == 3);

    dvect_remove(&vector, 2, &num);
    assert(num == 5);

    assert(dvect_get(&vector, 4, &num) == FALSE);

    assert(dvect_peek(&vector, &num) == TRUE);
    assert(num == 19);

    arena_destroy(&arena);
}

int main(void) {

    logger_system_init();

    test1();

    logger_system_cleanup();
    
    return 0;
}
