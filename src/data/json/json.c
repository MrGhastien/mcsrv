#include "data/json.h"
#include "json_internal.h"

#include "containers/dict.h"
#include "containers/vector.h"
#include "logger.h"
#include "utils/string.h"

#include <stdio.h>
#include <utils/str_builder.h>

static const char* TYPES[_JSON_COUNT] = {
    [JSON_NULL] = "JSON_NULL",
    [JSON_INT] = "JSON_INT",
    [JSON_FLOAT] = "JSON_FLOAT",
    [JSON_OBJECT] = "JSON_OBJECT",
    [JSON_ARRAY] = "JSON_ARRAY",
    [JSON_STRING] = "JSON_STRING",
    [JSON_BOOL] = "JSON_BOOL",
};

static void json_node_stringify(JSON* json, const JSONNode* node, StringBuilder* builder, size_t level);

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
        vector_init(node->data.array, 4, sizeof(JSONNode*));
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
            vector_get(node->data.array, i, &subnode);
            json_node_destroy(subnode);
        }
        vector_destroy(node->data.array);
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
    vector_add(array->data.array, &node);
    return node;
}

void json_node_addn(JSONNode* array, JSONNode* element) {
    if (!json_check_type(array, JSON_ARRAY))
        return;

    vector_add(array->data.array, &element);
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

static void add_padding(StringBuilder* builder, size_t level) {
    for (size_t i = 0; i < level; i++) {
        strbuild_appends(builder, "    ");
    }
}

static void
json_array_stringify(JSON* json, const JSONNode* array, StringBuilder* builder, size_t level) {
    Vector* vector = array->data.array;
    strbuild_appendc(builder, '[');
    for (size_t i = 0; i < vector->size; i++) {
        JSONNode* elem;
        strbuild_appendc(builder, '\n');
        add_padding(builder, level + 1);
        if (vector_get(vector, i, &elem))
            json_node_stringify(json, elem, builder, level + 1);
        if (i < vector->size - 1)
            strbuild_appendc(builder, ',');
    }
    strbuild_appendc(builder, '\n');
    add_padding(builder, level);
    strbuild_appendc(builder, ']');
}

struct stringify_data {
    JSON* json;
    StringBuilder* builder;
    bool linebreaks;
    size_t level;
};

static void foreach_stringify(const Dict* dict, size_t idx, void* key, void* value, void* data) {
    (void) dict;
    struct stringify_data* sdata = data;
    string* name = key;
    JSONNode* node = *(JSONNode**) value;

    add_padding(sdata->builder, sdata->level);

    strbuild_appendc(sdata->builder, '\"');
    strbuild_append(sdata->builder, name);
    strbuild_appends(sdata->builder, "\": ");
    json_node_stringify(sdata->json, node, sdata->builder, sdata->level);
    if (idx < dict->size - 1)
        strbuild_appendc(sdata->builder, ',');
    strbuild_appendc(sdata->builder, '\n');
}

static void json_node_stringify(JSON* json, const JSONNode* node, StringBuilder* builder, size_t level) {
    if (!node)
        return;
    switch (node->type) {
    case JSON_BOOL:
        strbuild_appends(builder, node->data.boolean ? "true" : "false");
        break;
    case JSON_INT: {
        strbuild_appendf(builder, "%li", node->data.number);
        break;
    }
    case JSON_FLOAT: {
        strbuild_appendf(builder, "%g", node->data.fnumber);
        break;
    }
    case JSON_NULL:
        strbuild_appends(builder, "null");
        break;
    case JSON_STRING:
        strbuild_appendc(builder, '\"');
        strbuild_append(builder, &node->data.string);
        strbuild_appendc(builder, '\"');
        break;
    case JSON_ARRAY:
        json_array_stringify(json, node, builder, level);
        break;

    case JSON_OBJECT: {
        struct stringify_data data = {.json = json, .builder = builder, .level = level + 1};
        strbuild_appends(builder, "{\n");
        dict_foreach(node->data.obj, &foreach_stringify, &data);
        add_padding(builder, level);
        strbuild_appendc(builder, '}');
        break;
    }
    default:
        break;
    }
}

void json_stringify(JSON* json, string* out, Arena* arena) {
    Arena scratch = *arena;
    StringBuilder builder = strbuild_create(&scratch);
    json_node_stringify(json, json->root, &builder, 0);
    strbuild_appendc(&builder, '\n');
    *out = strbuild_to_string(&builder, arena);
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
    if (!vector_get(node->data.array, idx, &out))
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
