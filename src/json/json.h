#ifndef JSON_H
#define JSON_H

#include "../utils/string.h"
#include "../containers/dict.h"
#include "../memory/arena.h"
#include "../containers/vector.h"

enum JSONType {
    JSON_STRING,
    JSON_INT,
    JSON_FLOAT,
    JSON_OBJECT,
    JSON_ARRAY,
    JSON_BOOL,
    JSON_NULL,
    _JSON_COUNT
};


typedef struct json_node {
    enum JSONType type;
    union {
        string string;
        Vector *array;
        bool boolean;
        long number;
        double fnumber;
        Dict* obj;
    } data;
} JSONNode;

typedef struct json {
    JSONNode* root;
    Arena arena;
} JSON;

void json_create(JSON* out_json);
void json_destroy(JSON* json);

JSONNode* json_node_put(JSON* json, JSONNode* obj, const string* name, enum JSONType type);

void json_stringify(JSON* json, string* out);

#endif /* ! JSON_H */
