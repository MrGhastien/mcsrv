#include "string.h"
#include "logger.h"
#include "memory/arena.h"
#include "utils/hash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

i32 str_compare_raw(const void* lhs, const void* rhs) {
    return str_compare(lhs, rhs);
}

const Comparator CMP_STRING = {
    .hfunc = &str_hash,
    .comp = &str_compare_raw,
};

string str_create(const char* cstr, Arena* arena) {
    size_t len = strlen(cstr);
    char* base = arena_allocate(arena, len + 1);
    memcpy(base, cstr, len);
    base[len] = 0;
    return (string){
        .base = base,
        .length = len,
    };
}

string str_create_const(const char* cstr) {
    u64 len = cstr == NULL ? 0 : strlen(cstr);
    return (string){
        .base = (char*) cstr,
        .length = len,
    };
}

string str_alloc(u64 length, Arena* arena) {
    char* base = arena_callocate(arena, length + 1);
    return (string){
        .base = base,
        .length = length,
    };
}

string str_create_copy(const string* str, Arena* arena) {
    string copy = str_alloc(str->length, arena);
    str_copy(&copy, str);
    return copy;
}

string str_create_from_buffer(const char* buf, u64 length, Arena* arena) {
    string str = str_alloc(length, arena);
    memcpy(str.base, buf, length);
    str.base[length] = 0;
    str.length = length;
    return str;
}

string str_copy_substring(const string* str, u64 begin, u64 end, Arena* arena) {
    u64 size = end - begin;
    string sub = str_alloc(size, arena);
    memcpy(sub.base, &str->base[begin], size);
    sub.base[size] = 0;
    return sub;
}

void str_set(string* str, const char* cstr) {
    char c;
    u32 i;
    for(i = 0; (c = cstr[i]) && i < str->length; i++)  {
        str->base[i] = cstr[i];
    }
    str->base[i] = 0;
    str->length = i;
}

void str_copy(string* dst, const string* src) {
    size_t len = src->length;

    memcpy(dst->base, src->base, len);
    dst->base[len] = 0;
    dst->length = len;
}

string str_concat(string* lhs, const string* rhs, Arena* arena) {
    u32 llen = lhs->length;
    u32 rlen = rhs->length;

    string res = {
        .base = arena_callocate(arena, sizeof(char) * (llen + rlen + 1)),
        .length = llen + rlen,
    };

    memcpy(res.base, lhs->base, llen);
    memcpy(res.base + llen, rhs->base, rlen);
    return res;
}

const char* str_printable_buffer(const string* str) {
    return str->base;
}

u64 str_hash(const void* str) {
    const string* cast_str = str;
    return default_hash((u8*) cast_str->base, cast_str->length);
}

i32 str_compare(const string* lhs, const string* rhs) {
    if(lhs == rhs)
        return 0;
    if(!lhs)
        return rhs ? -1 : 0;
    if(!rhs)
        return -1;
    if(lhs->length != rhs->length)
        return lhs->length - rhs->length;
    return memcmp(lhs->base, rhs->base, lhs->length);
}
