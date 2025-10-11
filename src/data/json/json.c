#include "data/json.h"
#include "containers/vector.h"
#include "json_internal.h"
#include "logger.h"
#include "utils/string.h"
#include <stdlib.h>
#include <string.h>

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
            log_fatalf("[JSON] Token of type %i cannot be a parent of other tokens !", token->type);
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

static enum JSONStatus check_parent_type(JSON* json, enum JSONType expected) {
    JSONToken* parent = get_current_node(json);
    if (!parent)
        return JSONE_MISSING_PARENT;

    if (parent->type != expected)
        return JSONE_INCOMPATIBLE_PARENT;
    return JSONE_OK;
}

JSON json_create(Arena* arena, u64 max_token_count) {
    JSON json;
    json.arena = arena;

    vect_init_dynamic(&json.tokens, arena, max_token_count, sizeof(JSONToken));
    vect_init(&json.stack, arena, 512, sizeof(i64));

    return json;
}

enum JSONStatus json_set_root(JSON* json, enum JSONType type) {
    if (vect_size(&json->stack) > 0)
        return JSONE_ROOT_ALREADY_PRESENT;

    JSONToken new_root = {
        .type = type,
    };
    vect_add(&json->tokens, &new_root);
    vect_add_imm(&json->stack, (i64)0);
    return JSONE_OK;
}

void append_token(JSON* json, JSONToken* new_token) {
    JSONToken* parent = get_current_node(json);
    if (!parent) {
        new_token->local_index        = 0;
        new_token->prev_sibling_index = -1;
        new_token->parent_index       = -1;
    } else {
        new_token->local_index = json_get_length(json);
        vect_peek(&json->stack, &new_token->parent_index);
        new_token->prev_sibling_index = parent->data.compound.last_child;
        parent->data.compound.size++;
        increment_parent_total_lengths(json);
    }
    vect_add(&json->tokens, new_token);
}

enum JSONStatus json_push_simple(JSON* json, enum JSONType type, union JSONSimpleValue value) {
    if (type != JSON_BOOL || type != JSON_INT || type != JSON_FLOAT)
        return JSONE_INCOMPATIBLE_TYPE;
    enum JSONStatus status = check_parent_type(json, JSON_ARRAY);
    if (status != JSONE_OK)
        return status;
    JSONToken new_token = {
        .type        = type,
        .data.simple = value,
    };
    append_token(json, &new_token);

    return JSONE_OK;
}
enum JSONStatus json_push_str(JSON* json, const string* str) {
    enum JSONStatus status = check_parent_type(json, JSON_ARRAY);
    if (status != JSONE_OK)
        return status;
    JSONToken new_token = {
        .type     = JSON_STRING,
        .data.str = str_create_copy(str, json->arena),
    };
    append_token(json, &new_token);

    return JSONE_OK;
}
enum JSONStatus json_push(JSON* json, enum JSONType type) {
    enum JSONStatus status = check_parent_type(json, JSON_ARRAY);
    if (status != JSONE_OK)
        return status;
    JSONToken new_token = {
        .type = type,
    };
    append_token(json, &new_token);
    return JSONE_OK;
}

enum JSONStatus
json_put_simple(JSON* json, const string* name, enum JSONType type, union JSONSimpleValue value) {
    enum JSONStatus status = check_parent_type(json, JSON_OBJECT);
    if (status != JSONE_OK)
        return status;
    JSONToken new_token = {
        .type        = type,
        .data.simple = value,
        .name        = str_create_copy(name, json->arena),
    };
    append_token(json, &new_token);
    return JSONE_OK;
}
enum JSONStatus json_put_str(JSON* json, const string* name, const string* str) {
    enum JSONStatus status = check_parent_type(json, JSON_OBJECT);
    if (status != JSONE_OK)
        return status;
    JSONToken new_token = {
        .type     = JSON_STRING,
        .data.str = str_create_copy(str, json->arena),
        .name     = str_create_copy(name, json->arena),
    };
    append_token(json, &new_token);
    return JSONE_OK;
}
enum JSONStatus json_put(JSON* json, const string* name, enum JSONType type) {
    enum JSONStatus status = check_parent_type(json, JSON_OBJECT);
    if (status != JSONE_OK)
        return status;
    JSONToken new_token = {
        .type = type,
        .name = str_create_copy(name, json->arena),
    };
    append_token(json, &new_token);
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

enum JSONStatus json_move_cstr(JSON* json, const char* path) {
    return json_move(json, str_view(path));
}

enum JSONStatus json_move(JSON* json, string path) {
    if (path.length == 0)
        return JSONE_OK;
    string view;
    i64 idx = 0;

    enum JSONStatus status = JSONE_OK;

    if (path.base[0] == '/') {
        vect_clear(&json->stack);
        vect_add_imm(&json->stack, (i64)0);
    }

    while ((idx = str_find_char(&path, '/')) >= 0) {
        if (idx == 0) {
            path.base++;
            path.length--;
            continue;
        }

        view = str_substring(&path, 0, idx);
        if (str_compare_cstr(&view, "..") == 0)
            status = json_move_to_parent(json);
        else if (str_compare_cstr(&view, ".") != 0) {
            status = json_move_to_name(json, &view);
        }
        if (status != JSONE_OK)
            return status;

        if ((u64) idx >= path.length - 1)
            path = STR_EMPTY;
        else {
            path.base += idx + 1;
            path.length -= idx + 1;
        }
    }
    if (path.length > 0)
        return json_move_to_name(json, &path);

    return JSONE_OK;
}

enum JSONStatus json_move_to_name(JSON* json, const string* name) {
    enum JSONStatus status = check_parent_type(json, JSON_OBJECT);
    if (status != JSONE_OK)
        return status;
    JSONToken* token = get_current_node(json);

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

    log_errorf("[JSON] Could not find a JSON token with name '%s'.", name->base);
    return JSONE_NOT_FOUND;
}
enum JSONStatus json_move_to_cstr(JSON* json, const char* name) {
    string str = str_view(name);
    return json_move_to_name(json, &str);
}

enum JSONStatus json_move_to_index(JSON* json, i32 index) {
    JSONToken* token = get_current_node(json);
    if (token == NULL)
        return JSONE_MISSING_PARENT;
    if (token->type != JSON_OBJECT && token->type != JSON_ARRAY)
        return JSONE_INCOMPATIBLE_TYPE;

    if (index < 0 || index >= token->data.compound.size) {
        log_errorf("[JSON] Index %i is out of the compound element bounds.", index);
        return JSONE_NOT_FOUND;
    }

    i64 idx;
    vect_peek(&json->stack, &idx);
    idx++;
    for (i32 i = 0; i < index; i++) {
        JSONToken* child_token = vect_ref(&json->tokens, idx);
        idx += get_total_length(child_token);
    }
    vect_add(&json->stack, &idx);
    return JSONE_OK;
}
enum JSONStatus json_move_to_parent(JSON* json) {
    if (vect_size(&json->stack) == 1)
        return JSONE_MISSING_PARENT;
    return vect_pop(&json->stack, NULL) ? JSONE_OK : JSONE_NOT_FOUND;
}
enum JSONStatus json_move_to_next_sibling(JSON* json) {
    JSONToken* token = get_current_node(json);
    i64 prev_index;
    enum JSONStatus status = JSONE_OK;
    if (!vect_pop(&json->stack, &prev_index))
        return JSONE_NOT_FOUND;
    if (token->local_index + 1 == json_get_length(json))
        status = JSONE_NOT_FOUND;
    else
        prev_index += get_total_length(token);
    vect_add(&json->stack, &prev_index);
    return status;
}
enum JSONStatus json_move_to_prev_sibling(JSON* json) {
    JSONToken* token = get_current_node(json);
    i64 prev_index;
    enum JSONStatus status = JSONE_OK;
    if (!vect_pop(&json->stack, &prev_index))
        return JSONE_NOT_FOUND;
    if (token->local_index == 0)
        status = JSONE_NOT_FOUND;
    else
        prev_index = token->prev_sibling_index;
    vect_add(&json->stack, &prev_index);
    return status;
}

i8 json_get_bool(JSON* json) {
    JSONToken* token = get_current_node(json);
    if (token->type != JSON_BOOL) {
        log_fatalf("[JSON] Cannot get boolean value of JSON token of type %i", token->type);
        abort();
    }

    return token->data.simple.boolean;
}
i64 json_get_int(JSON* json) {
    JSONToken* token = get_current_node(json);
    if (token->type != JSON_INT) {
        log_fatalf("[JSON] Cannot get integer value of JSON token of type %i", token->type);
        abort();
    }

    return token->data.simple.number;
}
f64 json_get_float(JSON* json) {
    JSONToken* token = get_current_node(json);
    if (token->type != JSON_FLOAT) {
        log_fatalf("[JSON] Cannot get float value of JSON token of type %i", token->type);
        abort();
    }

    return token->data.simple.fnumber;
}

i64 json_get_length(JSON* json) {
    JSONToken* token = get_current_node(json);
    if (token->type != JSON_OBJECT && token->type != JSON_ARRAY) {
        log_fatalf("[JSON] Cannot get length of JSON token of type %i", token->type);
        abort();
    }

    return token->data.compound.size;
}

string* json_get_name(JSON* json) {
    JSONToken* token = get_current_node(json);
    i64 parent_idx;
    if (!vect_get(&json->stack, json->stack.size - 2, &parent_idx))
        return NULL;
    JSONToken* parent = vect_ref(&json->tokens, parent_idx);
    if (parent->type != JSON_OBJECT)
        return NULL;
    return &token->name;
}
string* json_get_string(JSON* json) {
    JSONToken* token = get_current_node(json);
    if (token->type != JSON_STRING) {
        log_fatalf("[JSON] Cannot get string value of tag of type %i", token->type);
        abort();
    }
    return &token->data.str;
}
