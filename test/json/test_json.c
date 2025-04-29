#include <unity.h>
#include "containers/bytebuffer.h"
#include "logger.h"
#include "memory/memory.h"
#include "data/json.h"
#include "unity_internals.h"
#include "utils/iomux.h"
#include "utils/str_builder.h"
#include "platform/platform.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>

#include <string.h>

void setUp(void) {
    memory_stats_init();

    logger_system_init();
}

void tearDown(void) {
    memory_dump_stats();
    logger_system_cleanup();
}

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
    Arena arena = arena_create(1 << 18, BLK_TAG_UNKNOWN, INVALID_CHAIN);

    JSON json;
    IOMux mux = iomux_wrap_stdfile(f);
    enum JSONStatus status = json_parse(mux, &arena, &json);
    if (status != JSONE_OK) {
        arena_destroy(&arena);
        iomux_close(mux);
        return 2;
    }

    string out;
    json_to_string(&json, &arena, &out);

    printf("%s\n", cstr(&out));

    arena_destroy(&arena);
    iomux_close(mux);
    return 0;
}

static void test_dir(const char* path, bool error_test) {

    log_infof("JSON: Testing directory '%s'.", path);

    i32 error_count = 0;

    DIR* dir = opendir(path);
    Arena arena = arena_create(1 << 15, BLK_TAG_UNKNOWN, INVALID_CHAIN);

    struct dirent* element;
    while ((element = readdir(dir))) {

        if (!str_ends_with(element->d_name, ".json"))
            continue;
        StringBuilder builder = strbuild_create(&arena);
        strbuild_appends(&builder, path);
        strbuild_appendc(&builder, '/');
        strbuild_appends(&builder, element->d_name);
        string str_path = strbuild_to_string(&builder, &arena);
        log_debugf("JSON: Parsing %s...", str_path.base);

        i32 res = parse_json(str_path.base);
        if ((res && !error_test) || (!res && error_test))
            error_count++;

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

void test_good_dir(void) {
    test_dir("good", FALSE);
}

void test_bad_dir(void) {
    test_dir("bad", TRUE);
}

int main(void) {

    UNITY_BEGIN();

    RUN_TEST(test_good_dir);
    RUN_TEST(test_bad_dir);

    return UNITY_END();
}
