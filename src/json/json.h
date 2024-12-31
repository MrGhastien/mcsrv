#ifndef JSON_H
#define JSON_H

#include "utils/string.h"
#include "containers/dict.h"
#include "memory/arena.h"
#include "containers/vector.h"
#include "containers/bytebuffer.h"

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
    Arena* arena;
} JSON;

void json_create(JSON* out_json, Arena* arena);
void json_destroy(JSON* json);

JSONNode* json_node_create(JSON* json, enum JSONType type);

void json_set_root(JSON* json, JSONNode* node);

/**
   Add an entry `name` to the JSON object `obj` of type `type`. `obj` must be of type JSON_OBJECT.

   The `name` is shallow-copied in the tree's arena.

   @param json The JSON tree `obj` is a part of.
   @param obj The JSON object to add the entry to.
   @param name The name of the entry.
   @param type The type of value the entry will contain.

   @return A pointer to the entry`s value as a JSON node.
 */
JSONNode* json_node_puts(JSON* json, JSONNode* obj, const string* name, enum JSONType type);

JSONNode* json_node_put(JSON* json, JSONNode* obj, const char* name, enum JSONType type);

void json_node_putn(JSONNode* obj, const string* name, JSONNode* subnode);

/**
   Add an element to `array`. `array` must be of type JSON_ARRAY.

   The `name` is shallow-copied in the tree's arena.

   @param json The JSON tree `array` is a part of.
   @param obj The JSON array to add the element to.
   @param type The type of the elemt to add.

   @return A pointer to the new element.
 */
JSONNode* json_node_add(JSON* json, JSONNode* array, enum JSONType type);

void json_node_addn(JSONNode* array, JSONNode* element);

void json_set_str_direct(JSONNode* node, string* value);
void json_set_str(JSON* json, JSONNode* node, const string* value);
void json_set_cstr(JSON* json, JSONNode* node, const char* value);
void json_set_int(JSONNode* node, long value);
void json_set_double(JSONNode* node, double value);
void json_set_bool(JSONNode* node, bool value);

JSONNode* json_get_obj(const JSONNode* node, const string* name);
JSONNode* json_get_obj_cstr(const JSONNode* node, const char* name);
JSONNode* json_get_array(const JSONNode* node, u64 idx);
i64 json_get_length(const JSONNode* node);
string* json_get_str(JSONNode* node);
bool json_get_int(const JSONNode* node, i64* out);
bool json_get_float(const JSONNode* node, f64* out);
bool json_get_bool(const JSONNode* node, bool* out);
bool json_is_null(const JSONNode* node);

void json_stringify(JSON* json, string* out, u64 capacity, Arena* arena);

JSON json_parse(ByteBuffer* buffer, Arena* arena);

#endif /* ! JSON_H */
