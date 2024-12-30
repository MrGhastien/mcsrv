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


void strbuild_appendc(StringBuilder* builder, i32 c) {
    char chr = c;
    vector_add(&builder->chars, &chr);
}
i32 strbuild_appends(StringBuilder* builder, const char* cstr) {
    char c;
    i32 i = 0;
    while ((c = cstr[i])) {
        vector_add(&builder->chars, &c);
        i++;
    }
    return i;
}
void strbuild_append(StringBuilder* builder, const string* str) {
    for (u32 i = 0; i < str->length; i++) {
        char c = str->base[i];
        vector_add(&builder->chars, &c);
    }
}

void strbuild_append_buf(StringBuilder* builder, const char* buf, u64 size) {
    const char* chr_buf = buf;
    for (u64 i = 0; i < size; i++) {
        vector_add(&builder->chars, &chr_buf[i]);
    }
}

i32 strbuild_appendf(StringBuilder* builder, const char* format, ...) {
    va_list args;
    va_start(args, format);
    i32 res = strbuild_appendvf(builder, format, args);
    va_end(args);
    return res;
}

i32 strbuild_appendvf(StringBuilder* builder, const char* format, va_list args) {
    Arena scratch = *builder->arena;
    u64 size;
    const char* formatted = format_str(&scratch, format, args, &size);
    strbuild_append_buf(builder, formatted, size);
    return size;
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

void strbuild_insert_buf(StringBuilder* builder, u64 index, const char* buf, u64 size) {
    const char* chr_buf = buf;
    for (u64 i = 0; i < size; i++) {
        vector_insert(&builder->chars, &chr_buf[i], index + i);
    }
}

i32 strbuild_insertf(StringBuilder* builder, u64 index, const char* format, ...) {
    va_list args;
    va_start(args, format);
    i32 res = strbuild_insertvf(builder, index, format, args);
    va_end(args);
    return res;
}

i32 strbuild_insertvf(StringBuilder* builder, u64 index, const char* format, va_list args) {
    Arena scratch = *builder->arena;
    u64 size;
    const char* formatted = format_str(&scratch, format, args, &size);
    strbuild_insert_buf(builder, index, formatted, size);
    return size;
}

char strbuild_get(const StringBuilder* builder, u64 index) {
    char c;
    if (!vector_get(&builder->chars, index, &c))
        return -1;
    return c;
}
i32 strbuild_get_range(const StringBuilder* builder, char* out_text, u64 begin, u64 end) {
    if(begin >= builder->chars.size || end >= builder->chars.size || begin > end)
        return -1;
    u64 len = end - begin;
    u32 i;
    for (i = 0; i < len; ++i) {
       vector_get(&builder->chars, begin + i, &out_text[i]);
    }
    return i;
}

string strbuild_to_string(const StringBuilder* builder, Arena* arena) {
    u32 char_count = builder->chars.size;
    string str = str_alloc(char_count, arena);

    for (u32 i = 0; i < char_count; i++) {
        vector_get(&builder->chars, i, &str.base[i]);
    }

    return str;
}
