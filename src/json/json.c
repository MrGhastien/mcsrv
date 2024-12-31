#include "json.h"
#include "json_internal.h"

#include "containers/dict.h"
#include "containers/vector.h"
#include "logger.h"
#include "utils/string.h"

#include <stdio.h>

static const char* TYPES[_JSON_COUNT] = {
    [JSON_NULL] = "JSON_NULL",
    [JSON_INT] = "JSON_INT",
    [JSON_FLOAT] = "JSON_FLOAT",
    [JSON_OBJECT] = "JSON_OBJECT",
    [JSON_ARRAY] = "JSON_ARRAY",
    [JSON_STRING] = "JSON_STRING",
    [JSON_BOOL] = "JSON_BOOL",
};

static void json_node_stringify(JSON* json, const JSONNode* node, string* str, size_t level);

JSONNode* json_node_create(JSON* json, enum JSONType type) {
    Arena* arena = json->arena;

    JSONNode* node = arena_allocate(arena, sizeof(JSONNode));
    node->type = type;
    switch (type) {
    case JSON_OBJECT:
        node->data.obj = arena_allocate(arena, sizeof(Dict));
        dict_init(node->data.obj, &CMP_STRING, sizeof(string), sizeof(JSONNode*));
        break;
    case JSON_ARRAY:
        node->data.array = arena_allocate(arena, sizeof(Vector));
        vect_init_dynamic(node->data.array, arena, 4, sizeof(JSONNode*));
        break;
    case JSON_STRING:
        node->data.string = (string){0};
        break;
    case JSON_BOOL:
    case JSON_FLOAT:
    case JSON_INT:
    case JSON_NULL:
        node->data.number = 0;
        break;
    default:
        log_errorf("JSON: Invalid JSON type %i", type);
        break;
    }
    return node;
}

void json_create(JSON* out_json, Arena* arena) {
    out_json->arena = arena;
    out_json->root = NULL;
}

void json_set_root(JSON* json, JSONNode* node) {
    if (json->root) {
        log_warn("JSON already has a root node.");
        return;
    }
    json->root = node;
}

static void foreach_destroy(const Dict* dict, size_t idx, void* key, void* value, void* data) {
    (void) dict;
    (void) key;
    (void) data;
    (void) idx;
    JSONNode** subnode = value;
    json_node_destroy(*subnode);
}

void json_node_destroy(JSONNode* node) {
    switch (node->type) {
    case JSON_OBJECT:
        dict_foreach(node->data.obj, &foreach_destroy, NULL);
        dict_destroy(node->data.obj);
        break;
    case JSON_ARRAY:
        for (size_t i = 0; i < node->data.array->size; i++) {
            JSONNode* subnode;
            vect_get(node->data.array, i, &subnode);
            json_node_destroy(subnode);
        }
        break;
    case JSON_STRING:
        str_destroy(&node->data.string);
        break;
    default:
        break;
    }
}

void json_destroy(JSON* json) {
    if (json->root) {
        json_node_destroy(json->root);
        arena_free_ptr(json->arena, json->root);
    }
    json->arena = NULL;
    json->root = NULL;
}

static bool json_check_type(const JSONNode* node, enum JSONType type) {
    if (node->type != type) {
        const char* str_type = node->type < _JSON_COUNT ? TYPES[node->type] : "???";
        printf("Node %p (%s) is not of type '%s'!\n", node, str_type, TYPES[type]);
        return FALSE;
    }
    return TRUE;
}

JSONNode* json_node_puts(JSON* json, JSONNode* obj, const string* name, enum JSONType type) {
    if (!json_check_type(obj, JSON_OBJECT))
        return NULL;

    JSONNode* node = json_node_create(json, type);
    json_node_putn(obj, name, node);
    return node;
}

JSONNode* json_node_put(JSON* json, JSONNode* obj, const char* name, enum JSONType type) {
    // string str = str_alloc(name, json->arena);
    const string str = str_create_const(name);
    return json_node_puts(json, obj, &str, type);
}

void json_node_putn(JSONNode* obj, const string* name, JSONNode* subnode) {
    if (!json_check_type(obj, JSON_OBJECT))
        return;

    Dict* dict = obj->data.obj;

    dict_put(dict, name, &subnode);
}

JSONNode* json_node_add(JSON* json, JSONNode* array, enum JSONType type) {
    if (!json_check_type(array, JSON_ARRAY))
        return NULL;

    JSONNode* node = json_node_create(json, type);
    vect_add(array->data.array, &node);
    return node;
}

void json_node_addn(JSONNode* array, JSONNode* element) {
    if (!json_check_type(array, JSON_ARRAY))
        return;

    vect_add(array->data.array, &element);
}

void json_set_str_direct(JSONNode* node, string* value) {
    if (!json_check_type(node, JSON_STRING))
        return;
    if (node->data.string.base) {
        log_error("JSON: String is already set.");
        return;
    }

    node->data.string = *value;
}

void json_set_str(JSON* json, JSONNode* node, const string* value) {
    if (!json_check_type(node, JSON_STRING))
        return;
    if (node->data.string.base) {
        log_error("JSON: String is already set.");
        return;
    }

    node->data.string = str_create_copy(value, json->arena);
}

void json_set_cstr(JSON* json, JSONNode* node, const char* value) {
    if (!json_check_type(node, JSON_STRING))
        return;
    if (node->data.string.base) {
        log_error("JSON: String is already set.");
        return;
    }

    str_set(&node->data.string, value);
    node->data.string = str_create(value, json->arena);
}
void json_set_int(JSONNode* node, long value) {
    if (!json_check_type(node, JSON_INT))
        return;
    node->data.number = value;
}

void json_set_double(JSONNode* node, double value) {
    if (!json_check_type(node, JSON_FLOAT))
        return;
    node->data.fnumber = value;
}

void json_set_bool(JSONNode* node, bool value) {
    if (!json_check_type(node, JSON_BOOL))
        return;
    node->data.boolean = value;
}

static void add_padding(string* str, size_t level) {
    for (size_t i = 0; i < level; i++) {
        str_append(str, "    ");
    }
}

static void json_array_stringify(JSON* json, const JSONNode* array, string* str, size_t level) {
    Vector* vector = array->data.array;
    str_append(str, "[");
    for (size_t i = 0; i < vector->size; i++) {
        JSONNode* elem;
        str_append(str, "\n");
        add_padding(str, level + 1);
        if (vect_get(vector, i, &elem))
            json_node_stringify(json, elem, str, level + 1);
        if (i < vector->size - 1)
            str_append(str, ",");
    }
    str_append(str, "\n");
    add_padding(str, level);
    str_append(str, "]");
}

struct stringify_data {
    JSON* json;
    string* out_str;
    bool linebreaks;
    size_t level;
};

static void foreach_stringify(const Dict* dict, size_t idx, void* key, void* value, void* data) {
    (void) dict;
    struct stringify_data* sdata = data;
    string* name = key;
    JSONNode* node = *(JSONNode**) value;

    add_padding(sdata->out_str, sdata->level);

    str_append(sdata->out_str, "\"");
    str_concat(sdata->out_str, name);
    str_append(sdata->out_str, "\": ");
    json_node_stringify(sdata->json, node, sdata->out_str, sdata->level);
    if (idx < dict->size - 1)
        str_append(sdata->out_str, ",");
    str_append(sdata->out_str, "\n");
}

static void json_node_stringify(JSON* json, const JSONNode* node, string* str, size_t level) {
    if (!node)
        return;
    switch (node->type) {
    case JSON_BOOL:
        str_append(str, node->data.boolean ? "true" : "false");
        break;
    case JSON_INT: {
        char buf[32]; // longs are at most 20 decimal digits long
        snprintf(buf, 20, "%li", node->data.number);
        str_append(str, buf);
        break;
    }
    case JSON_FLOAT: {
        char buf[64];
        snprintf(buf, 64, "%g", node->data.fnumber);
        str_append(str, buf);
        break;
    }
    case JSON_NULL:
        str_append(str, "null");
        break;
    case JSON_STRING:
        str_append(str, "\"");
        str_concat(str, &node->data.string);
        str_append(str, "\"");
        break;
    case JSON_ARRAY:
        json_array_stringify(json, node, str, level);
        break;

    case JSON_OBJECT: {
        struct stringify_data data = {.json = json, .out_str = str, .level = level + 1};
        str_append(str, "{\n");
        dict_foreach(node->data.obj, &foreach_stringify, &data);
        add_padding(str, level);
        str_append(str, "}");
        break;
    }
    default:
        break;
    }
}

void json_stringify(JSON* json, string* out, u64 capacity, Arena* arena) {
    *out = str_alloc(capacity, arena);
    json_node_stringify(json, json->root, out, 0);
    str_append(out, "\n");
}

JSONNode* json_get_obj(const JSONNode* node, const string* name) {
    if (!json_check_type(node, JSON_OBJECT))
        return NULL;

    JSONNode* out;
    if (dict_get(node->data.obj, name, &out) < 0)
        return NULL;

    return out;
}

JSONNode* json_get_obj_cstr(const JSONNode* node, const char* name) {
    string str_name = str_create_const(name);
    return json_get_obj(node, &str_name);
}

JSONNode* json_get_array(const JSONNode* node, u64 idx) {
    if (!json_check_type(node, JSON_ARRAY))
        return NULL;

    JSONNode* out;
    if (!vect_get(node->data.array, idx, &out))
        return NULL;
    return out;
}

i64 json_get_length(const JSONNode* node) {
    switch (node->type) {
    case JSON_OBJECT:
        return node->data.obj->size;
    case JSON_ARRAY:
        return node->data.array->size;
    default:
        return -1;
    }
}

string* json_get_str(JSONNode* node) {
    if (!json_check_type(node, JSON_STRING))
        return NULL;
    return &node->data.string;
}

bool json_get_int(const JSONNode* node, i64* out) {
    if (!json_check_type(node, JSON_INT))
        return FALSE;
    *out = node->data.number;
    return TRUE;
}

bool json_get_float(const JSONNode* node, f64* out) {
    if (!json_check_type(node, JSON_FLOAT))
        return FALSE;
    *out = node->data.fnumber;
    return TRUE;
}

bool json_get_bool(const JSONNode* node, bool* out) {
    if (!json_check_type(node, JSON_BOOL))
        return FALSE;
    *out = node->data.boolean;
    return TRUE;
}

bool json_is_null(const JSONNode* node) {
    return node->type == JSON_NULL;
}
