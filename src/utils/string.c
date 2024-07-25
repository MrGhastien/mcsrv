#include "string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

string str_create_dynamic(const char* cstr) {
    size_t len = strlen(cstr);
    char* base = malloc(len + 1);
    memcpy(base, cstr, len);
    base[len] = 0;
    return (string){
        .base = base,
        .length = len,
        .capacity = len + 1,
        .fixed = FALSE,
    };
}

string str_create(const char* cstr, Arena* arena) {
    size_t len = strlen(cstr);
    char* base = arena_allocate(arena, len + 1);
    memcpy(base, cstr, len);
    base[len] = 0;
    return (string){
        .base = base,
        .length = len,
        .capacity = len + 1,
        .fixed = TRUE,
    };
}

string str_create_const(const char* cstr) {
    u64 len = strlen(cstr);
    return (string){
        .base = (char*) cstr,
        .length = len,
        .capacity = 0,
        .fixed = TRUE,
    };
}

string str_alloc(size_t capacity, Arena* arena) {
    char* base = arena_allocate(arena, capacity);
    return (string){
        .base = base,
        .length = 0,
        .capacity = capacity,
        .fixed = TRUE,
    };
}

string str_create_copy(const string* str, Arena* arena) {
    string copy = str_alloc(str->length + 1, arena);
    str_copy(&copy, str);
    return copy;
}

string str_create_from_buffer(const char* buf, u64 length, Arena* arena) {
    string str = str_alloc(length + 1, arena);
    memcpy(str.base, buf, length);
    str.base[length] = 0;
    str.length = length;
    return str;
}

string str_substring(const string* str, u64 begin, u64 end, Arena* arena) {
    u64 size = end - begin;
    string sub = str_alloc(size + 1, arena);
    memcpy(sub.base, &str->base[begin], size);
    sub.base[size] = 0;
    return sub;
}

bool str_is_const(const string* str) {
    return str->capacity == 0;
}

void str_destroy(string* str) {
    if (str_is_const(str))
        return;
    if (!str->fixed)
        free(str->base);
}

void str_set(string* str, const char* cstr) {
    if (str_is_const(str))
        return;
    size_t len = strlen(cstr);

    if (!str->fixed)
        str->base = realloc(str->base, len + 1);
    else if (len > str->capacity)
        return;

    memcpy(str->base, cstr, len + 1);
    str->length = len;
}

void str_copy(string* dst, const string* src) {
    if (str_is_const(dst))
        return;
    size_t len = src->length;

    if (!dst->fixed)
        dst->base = realloc(dst->base, len + 1);
    else if (len > dst->capacity)
        return;

    memcpy(dst->base, src->base, len);
    dst->base[len] = 0;
    dst->length = len;
}

void str_append(string* str, const char* cstr) {
    if (str_is_const(str)) {
        fputs("Attempted to concat something to a const string!", stderr);
        return;
    }
    size_t len = strlen(cstr);

    if (!str->fixed && str->length + len >= str->capacity) {
        size_t new_cap = str->capacity + (str->capacity >> 1);
        char* new_base = realloc(str->base, new_cap);
        if (new_base) {
            str->capacity = new_cap;
            str->base = new_base;
        }
    }

    str->length = strlcat(str->base, cstr, str->capacity);
    if (str->length >= str->capacity)
        str->length = str->capacity - 1;
    str->base[str->length] = 0;
}

void str_concat(string* lhs, const string* rhs) {
    if (str_is_const(lhs)) {
        fputs("Attempted to concat something to a const string!", stderr);
        return;
    }
    size_t len = rhs->length;

    if (!lhs->fixed && lhs->length + len >= lhs->capacity) {
        size_t new_cap = lhs->capacity + (lhs->capacity >> 1);
        char* new_base = realloc(lhs->base, new_cap);
        if (new_base) {
            lhs->capacity = new_cap;
            lhs->base = new_base;
        }
    }

    lhs->length = strlcat(lhs->base, rhs->base, lhs->capacity);
    if (lhs->length >= lhs->capacity)
        lhs->length = lhs->capacity - 1;
    lhs->base[lhs->length] = 0;
}
