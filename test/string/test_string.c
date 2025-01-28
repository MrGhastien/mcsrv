#include "memory/mem_tags.h"
#include "utils/string.h"
#include "utils/str_builder.h"

#include "logger.h"

#include <assert.h>

int main(void) {

    memory_stats_init();
    logger_system_init();
    Arena arena = arena_create(1 << 20, BLK_TAG_UNKNOWN);

    StringBuilder builder = strbuild_create(&arena);

    strbuild_appends(&builder, "hel");
    strbuild_appends(&builder, "rld");
    strbuild_appendc(&builder, '!');

    string tmp = str_view("lo wo");
    strbuild_insert(&builder, 3, &tmp);

    strbuild_appendf(&builder, "%s:%i", __FILE__, __LINE__);

    tmp = strbuild_to_string(&builder, &arena);
    log_infof("%s", cstr(&tmp));

    string cpy = str_alloc(50, &arena);
    str_copy(&cpy, &tmp);
    log_infof("%s", cstr(&cpy));
    assert(cpy.base[40] == 0);
    assert(cpy.base[45] == 0);

    str_set(&cpy, "salut les gens");
    log_infof("%s", cstr(&cpy));
    assert(cpy.base[30] == 0);
    assert(cpy.base[35] == 0);

    arena_destroy(&arena);
    logger_system_cleanup();

    return 0;
}
