#include "logger.h"
#include "memory/arena.h"
#include "data/nbt.h"
#include "utils/string.h"

#include <dirent.h>
#include <string.h>

/*
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
*/

static int test_write1(const char* file) {
    Arena arena = arena_create(1 << 24);

    NBT nbt = nbt_create(&arena, 1024);

    string tmp = str_create_const("test");
    nbt_put_simple(&nbt, &tmp, NBT_INT, (union NBTSimpleValue){.integer = 8274744});

    string out_path = str_create_const(file);
    nbt_write(&nbt, &out_path);

    arena_destroy(&arena);
    return 0;
}

static int test_write2(const char* file) {
    Arena arena = arena_create(1 << 24);

    NBT nbt = nbt_create(&arena, 1024);

    string tmp = str_create_const("data");
    nbt_put(&nbt, &tmp, NBT_COMPOUND);
    nbt_move_to_name(&nbt, &tmp);

    tmp = str_create_const("test");
    nbt_put_simple(&nbt, &tmp, NBT_INT, (union NBTSimpleValue){.integer = 8274744});

    string out_path = str_create_const(file);
    nbt_write(&nbt, &out_path);

    arena_destroy(&arena);
    return 0;
}

static int test_write3(const char* file) {
    Arena arena = arena_create(1 << 24);

    NBT nbt = nbt_create(&arena, 1024);

    string tmp = str_create_const("data");
    nbt_put(&nbt, &tmp, NBT_COMPOUND);
    nbt_move_to_name(&nbt, &tmp);

    tmp = str_create_const("prout jnjew&xxx");
    nbt_put(&nbt, &tmp, NBT_LIST);
    nbt_move_to_name(&nbt, &tmp);

    nbt_push_simple(&nbt, NBT_INT, (union NBTSimpleValue){.integer = 8274744});
    nbt_push_simple(&nbt, NBT_INT, (union NBTSimpleValue){.integer = 123440006});

    string out_path = str_create_const(file);
    nbt_write(&nbt, &out_path);

    arena_destroy(&arena);
    return 0;
}

static int test_read3(const char* file) {
    Arena arena = arena_create(1 << 24);
    NBT nbt;

    string in_path = str_create_const(file);
    nbt_parse(&arena, 1024, &in_path, &nbt);

    string out_path = str_create_const("inout3.nbt.gz");
    nbt_write(&nbt, &out_path);
    out_path = str_create_const("level.txt");
    nbt_write_snbt(&nbt, &out_path);

    arena_destroy(&arena);
    return 0;
}
/*
static void test_dir(const char* path, bool error_test) {

    log_infof("JSON: Testing directory '%s'.", path);

    i32 error_count = 0;

    DIR* dir = opendir(path);

    char buf[PATH_MAX];

    struct dirent* element;
    while ((element = readdir(dir))) {

        if (!str_ends_with(element->d_name, ".json"))
            continue;
        strlcpy(buf, path, PATH_MAX);
        strlcat(buf, "/", PATH_MAX);
        strlcat(buf, element->d_name, PATH_MAX);
        log_debugf("JSON: Parsing %s...", buf);

        i32 res = parse_json(buf);
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
*/

int main(void) {

    logger_system_init();

    test_write1("out1.nbt.gz");
    test_write2("out2.nbt.gz");
    test_write3("out3.nbt.gz");

    test_read3("out3.nbt.gz");

    logger_system_cleanup();

    return 0;
}
