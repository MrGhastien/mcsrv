#include "string.h"
#include <stdlib.h>
#include <string.h>

string str_init(const char* cstr, size_t length, Arena* arena) {
    bool allocated = FALSE;
    if (!arena) {
        arena = malloc(sizeof *arena);
        *arena = arena_create();
        allocated = TRUE;
    }

    string str = {
        .arena = arena,
        .length = length,
        .allocated = allocated,
    };

    str.base = arena_allocate(arena, str.length + 1);
    memcpy(str.base, cstr, str.length);
    str.base[str.length] = 0;
    return str;
}

string str_create(const char* cstr) {
    return str_init(cstr, strlen(cstr), NULL);
}

string str_create_with_buffer(const char* cstr, size_t length) {
    return str_init(cstr, length, NULL);
}

string str_alloc(const char* cstr, Arena* arena) {
    return str_init(cstr, strlen(cstr), arena);
}

void str_destroy(string* str) {
    arena_free(str->arena, (str->length + 1) * sizeof(char));
    if (str->allocated)
        free(str->arena);
}

void str_set(string* str, const char* cstr) {
    size_t len = strlen(cstr);

    if (len < str->length)
        arena_free(str->arena, str->length - len);
    else
        arena_allocate(str->arena, len - str->length);

    memcpy(str->base, cstr, len + 1);
}

void str_copy(string* dst, const string* src) {
    size_t len = src->length;

    if (len < dst->length)
        arena_free(dst->arena, dst->length - len);
    else
        arena_allocate(dst->arena, len - dst->length);

    memcpy(dst->base, src->base, len + 1);
}
