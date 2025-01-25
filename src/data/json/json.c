#include "data/json.h"
#include "containers/vector.h"
#include "json_internal.h"
#include "logger.h"
#include "utils/string.h"
#include <stdlib.h>

void increment_parent_total_lengths(JSON* json) {
    for (u32 i = 0; i < json->stack.size; i++) {
        u64 idx;
        vect_get(&json->stack, i, &idx);
        JSONToken* token = vect_ref(&json->tokens, idx);

        switch (token->type) {
        case JSON_OBJECT:
        case JSON_ARRAY:
            token->data.compound.total_node_length++;
            break;
        default:
            log_fatalf("JSON token of type %i cannot be a parent of other tokens !", token->type);
            abort();
            break;
        }
    }
}

static JSONToken* get_current_node(JSON* json) {
    i64 idx;
    if (!vect_peek(&json->stack, &idx))
        return NULL;

    return vect_ref(&json->tokens, idx);
}

static i32 get_total_length(const JSONToken* token) {
    if (!token)
        return 0;
    switch (token->type) {
    case JSON_OBJECT:
    case JSON_ARRAY:
        return token->data.compound.total_node_length + 1;
    default:
        return 1;
    }
}

JSON json_create(Arena* arena, u64 max_token_count) {
    JSON json;
    json.arena = arena;

    vect_init_dynamic(&json.tokens, arena, max_token_count, sizeof(JSONToken));
    vect_init(&json.stack, arena, 512, sizeof(i64));

    return json;
}

enum JSONStatus json_set_root(JSON* json, enum JSONType type) {
    if (vect_size(&json->stack) == 0)
        return JSONE_ROOT_ALREADY_PRESENT;

    JSONToken new_root = {
        .type = type,
    };
    vect_add(&json->tokens, &new_root);
    vect_add_imm(&json->stack, 0, i64);
    return JSONE_OK;
}

enum JSONStatus append_token(JSON* json, enum JSONType parent_type, JSONToken** out_token, i64* out_index) {
    i64 current_index = 0;
    JSONToken* current_token;
    if (!vect_peek(&json->stack, &current_index))
        current_token = NULL;
    else
        current_token = vect_ref(&json->tokens, current_index);

    if(parent_type != _JSON_COUNT) {
        if(!current_token)
            return JSONE_NO_ROOT;

        if(current_token->type != parent_type)
            return JSONE_INVALID_PARENT;
    }

    JSONToken new_token = {
        .parent_index = current_index,
    };
    i64 new_index = current_index + get_total_length(current_token);
    vect_insert(&json->tokens, &new_token, new_index);
    *out_token = vect_ref(&json->tokens, new_index);

    if(current_token)
        current_token->data.compound.size++;
    increment_parent_total_lengths(json);
    if(out_index)
        *out_index = new_index;

    return JSONE_OK;
}

enum JSONStatus json_push_simple(JSON* json, enum JSONType type, union JSONSimpleValue value) {
    if (type != JSON_BOOL || type != JSON_INT || type != JSON_FLOAT)
        return JSONE_INCOMPATIBLE_TYPE;
    JSONToken* new_token;
    enum JSONStatus status = append_token(json, JSON_ARRAY, &new_token, NULL);
    if (status != JSONE_OK)
        return status;

    *new_token = (JSONToken) {
        .type = type,
        .data.simple = value,
    };
    return JSONE_OK;
}
enum JSONStatus json_push_str(JSON* json, const string* str) {
    JSONToken* new_token;
    enum JSONStatus status = append_token(json, JSON_ARRAY, &new_token, NULL);
    if (status != JSONE_OK)
        return status;

    *new_token = (JSONToken) {
        .type = JSON_STRING,
        .data.str = str_create_copy(str, json->arena),
    };
    return JSONE_OK;
}
enum JSONStatus json_push(JSON* json, enum JSONType type) {
    JSONToken* new_token;
    enum JSONStatus status = append_token(json, JSON_ARRAY, &new_token, NULL);
    if (status != JSONE_OK)
        return status;

    *new_token = (JSONToken) {
        .type = type,
    };
    return JSONE_OK;
}

enum JSONStatus
json_put_simple(JSON* json, const string* name, enum JSONType type, union JSONSimpleValue value) {
    JSONToken* new_token;
    enum JSONStatus status = append_token(json, JSON_OBJECT, &new_token, NULL);
    if (status != JSONE_OK)
        return status;

    *new_token = (JSONToken) {
        .type = type,
        .data.simple = value,
        .name = str_create_copy(name, json->arena),
    };
    return JSONE_OK;
}
enum JSONStatus json_put_str(JSON* json, const string* name, const string* str) {
    JSONToken* new_token;
    enum JSONStatus status = append_token(json, JSON_OBJECT, &new_token, NULL);
    if (status != JSONE_OK)
        return status;

    *new_token = (JSONToken) {
        .type = JSON_STRING,
        .data.str = str_create_copy(str, json->arena),
        .name = str_create_copy(name, json->arena),
    };
    return JSONE_OK;
}
enum JSONStatus json_put(JSON* json, const string* name, enum JSONType type) {
    JSONToken* new_token;
    enum JSONStatus status = append_token(json, JSON_OBJECT, &new_token, NULL);
    if (status != JSONE_OK)
        return status;

    *new_token = (JSONToken) {
        .type = type,
        .name = str_create_copy(name, json->arena),
    };
    return JSONE_OK;
}

enum JSONStatus json_cstr_put_simple(JSON* json,
                                     const char* name,
                                     enum JSONType type,
                                     union JSONSimpleValue value) {
    string str = str_view(name);
    return json_put_simple(json, &str, type, value);
}
enum JSONStatus json_cstr_put_str(JSON* json, const char* name, const string* str) {
    string name_str = str_view(name);
    return json_put_str(json, &name_str, str);
}
enum JSONStatus json_cstr_put(JSON* json, const char* name, enum JSONType type) {
    string str = str_view(name);
    return json_put(json, &str, type);
}

enum JSONStatus json_set_bool(JSON* json, bool value) {
    JSONToken* token = get_current_node(json);
    if (token->type != JSON_BOOL)
        return JSONE_INCOMPATIBLE_TYPE;

    token->data.simple.boolean = value;
    return JSONE_OK;
}

enum JSONStatus json_set_int(JSON* json, i64 value) {
    JSONToken* token = get_current_node(json);
    if (token->type != JSON_INT)
        return JSONE_INCOMPATIBLE_TYPE;

    token->data.simple.number = value;
    return JSONE_OK;
}

enum JSONStatus json_set_float(JSON* json, f64 value) {
    JSONToken* token = get_current_node(json);
    if (token->type != JSON_FLOAT)
        return JSONE_INCOMPATIBLE_TYPE;

    token->data.simple.fnumber = value;
    return JSONE_OK;
}

enum JSONStatus json_write_file(const JSON* json, const string* path);
enum JSONStatus json_write(const JSON* json, IOMux multiplexer);

enum JSONStatus json_to_string(const JSON* json, Arena* arena, string* out_str);

/* === Parsing part === */

enum JSONStatus json_parse(IOMux multiplexer, Arena* arena, JSON* out_json);

enum JSONStatus json_move_to_name(JSON* json, const string* name) {
    JSONToken* token = get_current_node(json);
    if (token->type != JSON_OBJECT)
        return JSONE_INVALID_PARENT;

    i64 idx;
    vect_peek(&json->stack, &idx);
    idx++;
    for (i32 i = 0; i < token->data.compound.size; i++) {
        JSONToken* child_token = vect_ref(&json->tokens, idx);
        if (str_compare(&child_token->name, name) == 0) {
            vect_add(&json->stack, &idx);
            return JSONE_OK;
        }

        idx += get_total_length(child_token);
    }

    log_errorf("Could not find a JSON token with name %s.", name->base);
    return JSONE_NOT_FOUND;
}
enum JSONStatus json_move_to_cstr(JSON* json, const char* name) {
    string str = str_view(name);
    return json_move_to_name(json, &str);
}

enum JSONStatus json_move_to_index(JSON* json, i32 index) {
    JSONToken* token = get_current_node(json);

    if (index < 0 || index >= token->data.compound.size) {
        log_errorf("NBT: Index %i is out of the list's bounds.", index);
        return JSONE_NOT_FOUND;
    }

    i64 idx;
    vect_peek(&json->stack, &idx);
    for (i32 i = 0; i < index; i++) {
        JSONToken* child_token = vect_ref(&json->tokens, idx);
        idx += get_total_length(child_token);
    }
    vect_add(&json->stack, &idx);
    return JSONE_OK;
}
enum JSONStatus json_move_to_parent(JSON* json) {
    if (vect_size(&json->stack) == 1)
        return JSONE_INVALID_PARENT;
    return vect_pop(&json->stack, NULL) ? JSONE_OK : JSONE_NOT_FOUND;
}
enum JSONStatus json_move_to_next_sibling(JSON* json) {
    JSONToken* token = get_current_node(json);
    i64 prev_index;
    if (!vect_pop(&json->stack, &prev_index))
        return JSONE_NOT_FOUND;
    prev_index += get_total_length(token);
    vect_add(&json->stack, &prev_index);
    return JSONE_OK;
}
enum JSONStatus json_move_to_prev_sibling(JSON* json) {
    UNUSED(json);
    abort();
    return JSONE_NOT_FOUND;
}

i8 json_get_bool(JSON* json) {
    JSONToken* token = get_current_node(json);
    if (token->type != JSON_BOOL) {
        log_fatalf("Cannot get boolean value of JSON token of type %i", token->type);
        abort();
    }

    return token->data.simple.boolean;
}
i64 json_get_int(JSON* json) {
    JSONToken* token = get_current_node(json);
    if (token->type != JSON_INT) {
        log_fatalf("Cannot get integer value of JSON token of type %i", token->type);
        abort();
    }

    return token->data.simple.number;
}
f64 json_get_float(JSON* json) {
    JSONToken* token = get_current_node(json);
    if (token->type != JSON_FLOAT) {
        log_fatalf("Cannot get float value of JSON token of type %i", token->type);
        abort();
    }

    return token->data.simple.fnumber;
}

i64 json_get_length(JSON* json) {
    JSONToken* token = get_current_node(json);
    if (token->type != JSON_OBJECT || token->type != JSON_ARRAY) {
        log_fatalf("Cannot get length of JSON token of type %i", token->type);
        abort();
    }

    return token->data.compound.size;
}

string* json_get_name(JSON* json) {
    JSONToken* token = get_current_node(json);
    JSONToken* parent = vect_ref(&json->stack, json->stack.size - 2);
    if (!parent || parent->type != JSON_OBJECT)
        return NULL;
    return &token->name;
}
string* json_get_string(JSON* json) {
    JSONToken* token = get_current_node(json);
    if (token->type != JSON_STRING) {
        log_fatalf("Cannot get string value of tag of type %i", token->type);
        abort();
    }
    return &token->data.str;
}
