#include "json.h"
#include "../utils/string.h"

static void json_node_destroy(JSONNode* node);

static JSONNode* json_node_create(JSON* json, enum JSONType type) {
    Arena* arena = &json->arena;

    JSONNode* node = arena_allocate(arena, sizeof(JSONNode));
    node->type = type;
    switch (type) {
    case JSON_OBJECT:
        node->data.obj = arena_allocate(arena, sizeof(Dict));
        dict_init(node->data.obj, sizeof(JSONNode));
        break;
    case JSON_ARRAY:
        node->data.array = arena_allocate(arena, sizeof(Vector));
        vector_init(node->data.array, 2, sizeof(JSONNode));
        break;
    case JSON_STRING:
        node->data.string = str_create("");
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

void json_create(JSON* out_json) {
    out_json->arena = arena_create();
    json_node_create(out_json, JSON_OBJECT);
}

static void foreach_destroy(char* key, void* value, void* data) {
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
    json_node_destroy(json->root);
    arena_destroy(&json->arena);
}

void json_node_put(JSON* json, JSONNode* obj, const string* name, enum JSONType type) {
    JSONNode* node = json_node_create(json, type);
    Dict* dict = obj->data.obj;

    dict_put_imm(dict, name, node, JSONNode*);
}
