#include "containers/bytebuffer.h"
#include "logger.h"
#include "memory/arena.h"
#include "data/json.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

static bool str_ends_with(const char* str, const char* substr) {
    u64 length = strlen(str);
    u64 sublength = strlen(substr);
    if (sublength > length)
        return FALSE;

    u64 i = 0;
    while (i < sublength && str[length - i] == substr[sublength - i]) {
        i++;
    }
    return i == sublength;
}

int parse_json(const char* file) {

    FILE* f = fopen(file, "r");
    if (!f) {
        log_errorf("JSON: Failed to open file '%s': %s.", file, strerror(errno));
        return 1;
    }

    ByteBuffer buffer = bytebuf_create(256);
    u8 tmp[64];
    while (!feof(f)) {
        u64 size = fread(tmp, 1, 64, f);
        bytebuf_write(&buffer, tmp, size);
        if (size < 64)
            break;
    }

    Arena arena = arena_create(1 << 15);

    JSON json = json_parse(&buffer, &arena);
    if (!json.arena) {
        arena_destroy(&arena);
        fclose(f);
        bytebuf_destroy(&buffer);
        return 2;
    }

    /*
    string out;
    json_stringify(&json, &out, 1 << 14, &arena);

    printf("%s\n", out.base);
    */

    json_destroy(&json);

    arena_destroy(&arena);
    fclose(f);
    bytebuf_destroy(&buffer);
    return 0;
}

static void test_dir(const char* path, bool error_test) {

    log_infof("JSON: Testing directory '%s'.", path);

    i32 error_count = 0;

    DIR* dir = opendir(path);

    struct dirent* element;
    while ((element = readdir(dir))) {

        if (!str_ends_with(element->d_name, ".json"))
            continue;
        string str_path = str_create_dynamic(path);
        str_appendc(&str_path, '/');
        str_append(&str_path, element->d_name);
        log_debugf("JSON: Parsing %s...", str_path.base);

        i32 res = parse_json(str_path.base);
        if ((res && !error_test) || (!res && error_test))
            error_count++;

        str_destroy(&str_path);
    }

    closedir(dir);

    if (error_count) {
        if (error_test)
            log_errorf("JSON: %i tests have not catched errors !", error_count);
        else
            log_errorf("JSON: %i tests failed !", error_count);
    } else {
        if (error_test)
            log_info("JSON: All tests catched errors.");
        else
            log_info("JSON: All tests passed.");
    }
}

int main(void) {

    logger_system_init();

    test_dir("good", FALSE);
    test_dir("bad", TRUE);

    logger_system_cleanup();

    return 0;
}
