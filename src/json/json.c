#include "json.h"
#include "../utils/string.h"

static void json_node_destroy(JSONNode* node);

static JSONNode* json_node_create(JSON* json, enum JSONType type) {
    Arena* arena = json->arena;

    JSONNode* node = arena_allocate(arena, sizeof(JSONNode));
    node->type = type;
    switch (type) {
    case JSON_OBJECT:
        node->data.obj = arena_allocate(arena, sizeof(Dict));
        dict_init_with_arena(node->data.obj, arena, sizeof(string), sizeof(JSONNode*));
        break;
    case JSON_ARRAY:
        node->data.array = arena_allocate(arena, sizeof(Vector));
        vector_init_with_arena(node->data.array, arena, 2, sizeof(JSONNode*));
        break;
    case JSON_STRING:
        node->data.string = str_alloc("", arena);
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
    json_node_create(out_json, JSON_OBJECT);
}

static void foreach_destroy(const Dict* dict, void* key, void* value, void* data) {
    (void)dict;
    (void)key;
    (void)data;
    json_node_destroy(value);
}

static void json_node_destroy(JSONNode* node) {
    switch (node->type) {
    case JSON_OBJECT:
        dict_foreach(node->data.obj, &foreach_destroy, NULL);
        dict_destroy(node->data.obj);
        break;
    case JSON_ARRAY:
        for (size_t i = 0; i < node->data.array->size; i++) {
            json_node_destroy(vector_ref(node->data.array, i));
        }
        vector_clear(node->data.array);
        break;
    case JSON_STRING:
        str_destroy(&node->data.string);
    default:
        break;
    }
}

void json_destroy(JSON* json) {
    //json_node_destroy(json->root);
    arena_free_ptr(json->arena, json->root);
}

JSONNode* json_node_puts(JSON* json, JSONNode* obj, const string* name, enum JSONType type) {
    if(obj->type != JSON_OBJECT)
        return NULL;
    JSONNode* node = json_node_create(json, type);
    Dict* dict = obj->data.obj;

    dict_put(dict, name, &node);
    return node;
}

JSONNode* json_node_put(JSON* json, JSONNode* obj, const char* name, enum JSONType type) {
    string str = str_alloc(name, json->arena);
    return json_node_puts(json, obj, &str, type);
}

JSONNode* json_node_add(JSON *json, JSONNode *array, enum JSONType type) {
    if(array->type != JSON_ARRAY)
        return NULL;

    JSONNode* node = json_node_create(json, type);
    vector_add(array->data.array, &node);
    return node;
}

void json_set_str(JSON* json, JSONNode* node, const string* value) {
    if(node->type != JSON_STRING)
        return;

    str_copy(&node->data.string, value);
}

void json_set_cstr(JSON* json, JSONNode* node, const char* value) {
    if(node->type != JSON_STRING)
        return;

    str_set(&node->data.string, value);
}
void json_set_int(JSON* json, JSONNode* node, long value);
void json_set_double(JSON* json, JSONNode* node, double value);
void json_set_bool(JSON* json, JSONNode* node, bool value);
