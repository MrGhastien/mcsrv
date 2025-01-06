#include "containers/vector.h"
#include "logger.h"
#include "memory/arena.h"
#include <assert.h>

static void test1(void) {
    Arena arena = arena_create(1 << 20);

    Vector vector;
    vect_init_dynamic(&vector, &arena, 4, sizeof(u64));

    vect_add_imm(&vector, 7837, u64);
    vect_add_imm(&vector, 1, u64);
    vect_add_imm(&vector, 3, u64);
    vect_add_imm(&vector, 5, u64);
    vect_add_imm(&vector, 7, u64);
    vect_add_imm(&vector, 19, u64);
    vect_add_imm(&vector, 9, u64);

    u64 num;
    vect_get(&vector, 4, &num);
    assert(num == 7);

    vect_pop(&vector, &num);
    assert(num == 9);

    vect_remove(&vector, 2, &num);
    assert(num == 3);

    vect_remove(&vector, 2, &num);
    assert(num == 5);

    assert(vect_get(&vector, 4, &num) == FALSE);

    assert(vect_peek(&vector, &num) == TRUE);
    assert(num == 19);

    arena_destroy(&arena);
}

int main(void) {

    logger_system_init();

    test1();

    logger_system_cleanup();
    
    return 0;
}
