#ifndef STRING_H
#define STRING_H

#include "../definitions.h"
#include "../memory/arena.h"
#include <stddef.h>

typedef struct str {
    char* base;
    size_t length;
    size_t capacity;
    bool fixed;
} string;

string str_init(const char* cstr, size_t length, size_t capacity, Arena* arena);
string str_create(const char* cstr);
string str_create_const(const char* cstr);
string str_alloc(size_t capacity, Arena* arena);

void str_destroy(string* str);

bool str_is_const(const string* str);

/**
   Set the string to the passed C string.
   Adequatly resize the buffer to fit the content.
 */
void str_set(string* str, const char* cstr);
void str_copy(string* dst, const string* src);

void str_append(string* str, const char* cstr);
void str_concat(string* lhs, const string* rhs);

#endif /* ! STRING_H */
