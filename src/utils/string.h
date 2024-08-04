#ifndef STRING_H
#define STRING_H

#include "definitions.h"
#include "memory/arena.h"
#include <stddef.h>

typedef struct str {
    char* base;
    u64 length;
    u64 capacity;
    bool fixed;
} string;

/**
 * Create a string from the provided C string.
 * The contents are copied into a newly allocated buffer.
 */
string str_create_dynamic(const char* cstr);

/**
 * Create a string from the provided C string,
 * using `arena` to allocate the underlying buffer.
 *
 * The contents are copied.
 */
string str_create(const char* cstr, Arena* arena);

/**
 * Create an immutable string from the given C string.
 * The contents are NOT copied, instead the given C string is used directly.
 */
string str_create_const(const char* cstr);

/**
 * Create a new string from an other one, using ARENA to allocate the underlying buffer.
 */
string str_create_copy(const string* str, Arena* arena);

string str_create_from_buffer(const char* buf, u64 length, Arena* arena);

string str_alloc(size_t capacity, Arena* arena);
string str_substring(const string* str, u64 begin, u64 end, Arena* arena);

void str_destroy(string* str);

bool str_is_const(const string* str);

/**
   Set the string to the passed C string.
   Adequatly resize the buffer to fit the content.
 */
void str_set(string* str, const char* cstr);
void str_copy(string* dst, const string* src);

void str_append(string* str, const char* cstr);
void str_appendc(string* str, char c);
void str_concat(string* lhs, const string* rhs);

#endif /* ! STRING_H */
