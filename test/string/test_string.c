#include "utils/string.h"
#include "utils/str_builder.h"

#include "logger.h"

int main(void) {

    logger_system_init();
    Arena arena = arena_create(1 << 20);

    StringBuilder builder = strbuild_create(&arena);

    strbuild_appends(&builder, "hel");
    strbuild_appends(&builder, "rld");
    strbuild_appendc(&builder, '!');

    string tmp = str_create_const("lo wo");
    strbuild_insert(&builder, 3, &tmp);

    tmp = strbuild_to_string(&builder, &arena);
    log_infof("%s", str_printable_buffer(&tmp));

    arena_destroy(&arena);
    logger_system_cleanup();

    return 0;
}
