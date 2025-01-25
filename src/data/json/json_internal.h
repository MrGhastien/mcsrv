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
        } compound;
    } data;
    string name;
    i64 parent_index;
} JSONToken;

/**
 * Adds a token to a JSON tree.
 * 
 */
enum JSONStatus append_token(JSON* json, enum JSONType parent_type, JSONToken** out_token, i64* out_index);

void increment_parent_total_lengths(JSON* json);

#endif /* ! JSON_INTERNAL_H */
