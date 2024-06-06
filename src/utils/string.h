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
string str_alloc(const char* cstr, Arena* arena);

void str_destroy(string* str);

/**
   Set the string to the passed C string.
   Adequatly resize the buffer to fit the content.
 */
void str_set(string* str, const char* cstr);
void str_copy(string* dst, const string* src);

#endif /* ! STRING_H */
