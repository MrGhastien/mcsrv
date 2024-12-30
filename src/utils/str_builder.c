#include "str_builder.h"
#include "containers/vector.h"
#include "memory/arena.h"

#include <logger.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

StringBuilder strbuild_create(Arena* arena) {
    StringBuilder builder = {
        .arena = arena,
    };
    vector_init_fixed(&builder.chars, arena, 512, sizeof(char));
    return builder;
}

static char* format_str(Arena* scratch, const char* format,  va_list args, u64* out_size) {
    va_list args_copy;
    va_copy(args_copy, args);
    i32 res = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    if(res < 0) {
        log_errorf("Failed to append formatted string: %s", strerror(errno));
        return NULL;
    }

    if(out_size)
        *out_size = res;
    char* buf = arena_allocate(scratch, res + 1);
    vsnprintf(buf, res + 1, format, args);
    return buf;
}

void strbuild_appendc(StringBuilder* builder, i32 c) {
    char chr = c;
    vector_add(&builder->chars, &chr);
}
void strbuild_appends(StringBuilder* builder, const char* cstr) {
    char c;
    while ((c = *cstr)) {
        vector_add(&builder->chars, &c);
        cstr++;
    }
}
void strbuild_append(StringBuilder* builder, const string* str) {
    for (u32 i = 0; i < str->length; i++) {
        char c = str->base[i];
        vector_add(&builder->chars, &c);
    }
}

void strbuild_appendf(StringBuilder* builder, const char* format, ...) {
    Arena scratch = *builder->arena;
    va_list args;
    va_start(args, format);
    const char* formatted = format_str(&scratch, format, args, NULL);
    va_end(args);
    strbuild_appends(builder, formatted);
}

void strbuild_insertc(StringBuilder* builder, u64 index, i32 c) {
    vector_insert(&builder->chars, &c, index);
}
void strbuild_inserts(StringBuilder* builder, u64 index, const char* cstr) {
    char c;
    for (u32 i = 0; (c = cstr[i]); i++) {
        vector_insert(&builder->chars, &c, index + i);
    }
}
void strbuild_insert(StringBuilder* builder, u64 index, const string* str) {
    for (u32 i = 0; i < str->length; i++) {
        char c = str->base[i];
        vector_insert(&builder->chars, &c, index + i);
    }
}

void strbuild_insertf(StringBuilder* builder, u64 index, const char* format, ...) {
    Arena scratch = *builder->arena;
    va_list args;
    va_start(args, format);
    const char* formatted = format_str(&scratch, format, args, NULL);
    va_end(args);
    strbuild_inserts(builder, index, formatted);
}

string strbuild_to_string(const StringBuilder* builder, Arena* arena) {
    u32 char_count = builder->chars.size;
    string str = str_alloc(char_count, arena);

    for (u32 i = 0; i < char_count; i++) {
        vector_get(&builder->chars, i, &str.base[i]);
    }

    return str;
}
