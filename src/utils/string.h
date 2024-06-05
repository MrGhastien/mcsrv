#ifndef STRING_H
#define STRING_H

#include "../definitions.h"
#include <stddef.h>

typedef struct str {
    char* chars;
    size_t length;
    bool allocated;
} string;

string str_create(char* cstr);
string str_create_with_buffer(char* cstr, size_t length);

void str_destroy(string* str);



#endif /* ! STRING_H */
