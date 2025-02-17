#ifndef JSON_INTERNAL_H
#define JSON_INTERNAL_H

#include "data/json.h"

typedef struct json_token {
    enum JSONType type;
    union {
        union JSONSimpleValue simple;
        string str;
        struct json_compound {
            i32 size;
            i32 total_node_length;
            i64 last_child;
        } compound;
    } data;
    string name;
    i64 parent_index;
    i64 local_index;
    i64 prev_sibling_index;
} JSONToken;

void append_token(JSON* json, JSONToken* new_token);

void increment_parent_total_lengths(JSON* json);

#endif /* ! JSON_INTERNAL_H */
