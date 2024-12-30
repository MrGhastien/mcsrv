#include "str_builder.h"
#include "containers/vector.h"
#include "memory/arena.h"

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

string strbuild_to_string(StringBuilder* builder) {
    u32 char_count = builder->chars.size;
    string str = {
        .base = arena_callocate(builder->arena, sizeof(char) * (char_count + 1)),
        .length = char_count,
    };

    for(u32 i = 0; i < char_count; i++) {
        vector_get(&builder->chars, i, &str.base[i]);
    }
    
    return str;
}
