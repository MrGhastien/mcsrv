#include "string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

string str_init(const char* cstr, size_t length, size_t capacity, Arena* arena) {
    string str;
    if (capacity == 0)
        str.base = (char*) cstr;
    else if (!arena)
        str.base = malloc(capacity);
    else
        str.base = arena_allocate(arena, capacity);

    if (!str.base) {
        str.fixed    = TRUE;
        str.capacity = 0;
        str.length   = 0;
        return str;
    }

    str.fixed    = capacity == 0 || arena != NULL;
    str.length   = length;
    str.capacity = capacity;

    if (!str_is_const(&str)) {
        str.base[str.length] = 0;
        if (cstr)
            memcpy(str.base, cstr, length);
    }
    return str;
}

string str_create(const char* cstr) {
    size_t len = strlen(cstr);
    return str_init(cstr, len, len + 1, NULL);
}

string str_create_const(const char* cstr) {
    return str_init(cstr, strlen(cstr), 0, NULL);
}

string str_alloc(size_t capacity, Arena* arena) {
    return str_init(NULL, 0, capacity, arena);
}

string str_create_from(const string* str, Arena* arena) {
    return str_init(str->base, str->length, str->capacity, arena);
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

    memcpy(dst->base, src->base, len + 1);
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
            str->base     = new_base;
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
            lhs->base     = new_base;
        }
    }

    lhs->length = strlcat(lhs->base, rhs->base, lhs->capacity);
    if (lhs->length >= lhs->capacity)
        lhs->length = lhs->capacity - 1;
    lhs->base[lhs->length] = 0;
}
