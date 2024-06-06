#ifndef STRING_H
#define STRING_H

#include "../definitions.h"
#include "../memory/arena.h"
#include <stddef.h>

typedef struct str {
    Arena* arena;
    char* base;
    size_t length;
    bool allocated;
} string;

string str_init(const char* cstr, size_t length, Arena* arena);
string str_create(const char* cstr);
string str_create_with_buffer(const char* cstr, size_t length);
string str_create_with_arena(const char* cstr, Arena* arena);

void str_destroy(string* str);

void str_set(string* str, const char* cstr);

#endif /* ! STRING_H */
