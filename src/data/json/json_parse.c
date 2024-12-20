#include "data/json.h"
#include "json_internal.h"

#include "containers/bytebuffer.h"
#include "containers/vector.h"
#include "logger.h"
#include "memory/arena.h"
#include "utils/string.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JSON_MAX_NESTING 32

enum JSONToken {
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_COMMA,
    TOK_COLON,
    TOK_NULL,
    TOK_ERROR,
    TOK_EOF,

    TOK_STRING,
    TOK_INT,
    TOK_FLOAT,
    TOK_BOOL,
    _TOK_COUNT
};

typedef union tok_value {
    bool boolean;
    i64 integer;
    f64 floating;
    string str;
} TokenValue;

typedef struct parse_info {
    Vector* tok_list;
    Vector* tok_values;
    u64 idx;
    u64 val_idx;
    JSON* json;
} ParsingInfo;

static char* names[_TOK_COUNT] = {
    [TOK_LBRACE] = "TOK_LBRACE",
    [TOK_RBRACE] = "TOK_RBRACE",
    [TOK_LBRACKET] = "TOK_LBRACKET",
    [TOK_RBRACKET] = "TOK_RBRACKET",
    [TOK_COMMA] = "TOK_COMMA",
    [TOK_COLON] = "TOK_COLON",
    [TOK_NULL] = "TOK_NULL",
    [TOK_ERROR] = "TOK_ERROR",
    [TOK_EOF] = "TOK_EOF",
    [TOK_STRING] = "TOK_STRING",
    [TOK_INT] = "TOK_INT",
    [TOK_FLOAT] = "TOK_FLOAT",
    [TOK_BOOL] = "TOK_BOOL",
};

static enum JSONType types[] = {
    [TOK_RBRACE] = _JSON_COUNT,
    [TOK_RBRACKET] = _JSON_COUNT,
    [TOK_COMMA] = _JSON_COUNT,
    [TOK_COLON] = _JSON_COUNT,
    [TOK_ERROR] = _JSON_COUNT,
    [TOK_EOF] = _JSON_COUNT,

    [TOK_LBRACE] = JSON_OBJECT,
    [TOK_LBRACKET] = JSON_ARRAY,
    [TOK_STRING] = JSON_STRING,
    [TOK_INT] = JSON_INT,
    [TOK_FLOAT] = JSON_FLOAT,
    [TOK_BOOL] = JSON_BOOL,
    [TOK_NULL] = JSON_NULL,
};

/* === TOKENIZING === */

static bool token_has_value(enum JSONToken token) {
    return token >= TOK_STRING;
}

static bool lex_string(ByteBuffer* buffer, TokenValue* value, Arena* arena) {
    char c;
    string tmp = str_create_dynamic("");
    bytebuf_read(buffer, 1, NULL);
    while (bytebuf_read(buffer, 1, &c) == 1 && c != '"') {
        str_appendc(&tmp, c);
        if (c == '\\') {
            bytebuf_read(buffer, 1, NULL);
            str_appendc(&tmp, c);
        }
    }
    if (c != '"')
        return FALSE;

    value->str = str_create_copy(&tmp, arena);
    str_destroy(&tmp);

    return TRUE;
}

static bool is_valid_number_char(char c, bool frac, bool exponent) {
    return (c >= '0' && c <= '9') || (!frac && c == '.') || (!exponent && (c == 'e' || c == 'E'));
}

static enum JSONToken lex_number(ByteBuffer* buffer, TokenValue* value, Arena* arena) {
    bool frac = FALSE;
    bool exponent = FALSE;

    arena_save(arena);
    string str = str_alloc(32, arena);
    char c;
    while (bytebuf_peek(buffer, 1, &c) == 1 && is_valid_number_char(c, frac, exponent)) {
        bytebuf_read(buffer, 1, &c);
        str_appendc(&str, c);
    }
    if (c == '.' && frac)
        return TOK_ERROR;
    if ((c == 'e' || c == 'E') && exponent)
        return TOK_ERROR;

    enum JSONToken token;
    if (frac) {
        value->floating = strtod(str.base, NULL);
        token = TOK_FLOAT;
    } else {
        value->integer = strtol(str.base, NULL, 10);
        token = TOK_INT;
    }

    arena_restore(arena);
    return token;
}

static bool lex_boolean(ByteBuffer* buffer, TokenValue* value, bool expected) {
    u8 tmp[5];
    if (expected) {
        if (bytebuf_read(buffer, 4, tmp) != 4)
            return FALSE;
        if (memcmp(tmp, "true", 4) != 0)
            return FALSE;
        value->boolean = TRUE;
    } else {
        if (bytebuf_read(buffer, 5, tmp) != 5)
            return FALSE;
        if (memcmp(tmp, "false", 5) != 0)
            return FALSE;
        value->boolean = FALSE;
    }
    return TRUE;
}

static bool lex_null(ByteBuffer* buffer) {
    u8 tmp[4];
    if (bytebuf_read(buffer, 4, tmp) != 4)
        return FALSE;
    return memcmp(tmp, "null", 4) == 0;
}

static enum JSONToken lex_token(ByteBuffer* buffer, TokenValue* value, Arena* arena) {
    char c;
    if (bytebuf_peek(buffer, 1, &c) < 1)
        return TOK_EOF;
    while (isspace(c)) {
        bytebuf_read(buffer, 1, NULL);
        if (bytebuf_peek(buffer, 1, &c) < 1)
            return TOK_EOF;
    }

    switch (c) {
    case '{':
        bytebuf_read(buffer, 1, NULL);
        return TOK_LBRACE;
    case '}':
        bytebuf_read(buffer, 1, NULL);
        return TOK_RBRACE;
    case '[':
        bytebuf_read(buffer, 1, NULL);
        return TOK_LBRACKET;
    case ']':
        bytebuf_read(buffer, 1, NULL);
        return TOK_RBRACKET;
    case ',':
        bytebuf_read(buffer, 1, NULL);
        return TOK_COMMA;
    case ':':
        bytebuf_read(buffer, 1, NULL);
        return TOK_COLON;
    case '"':
        if (lex_string(buffer, value, arena))
            return TOK_STRING;
        else
            return TOK_ERROR;
    case 't':
        if (lex_boolean(buffer, value, TRUE))
            return TOK_BOOL;
        else
            return TOK_ERROR;
    case 'f':
        if (lex_boolean(buffer, value, FALSE))
            return TOK_BOOL;
        else
            return TOK_ERROR;
    case 'n':
        return lex_null(buffer) ? TOK_NULL : TOK_ERROR;
    case 0:
        return TOK_EOF;
    default:
        if ((c >= '0' && c <= '9') || c == '-')
            return lex_number(buffer, value, arena);
        log_errorf("JSON: Lexical error: Unknown character '%c'.", c);
        return TOK_ERROR;
    }
}

static bool json_lex(ByteBuffer* buffer, Vector* tok_list, Vector* tok_values, Arena* arena) {
    enum JSONToken token;
    TokenValue value;
    do {
        token = lex_token(buffer, &value, arena);
        vector_add(tok_list, &token);
        if (token_has_value(token))
            vector_add(tok_values, &value);
    } while (token != TOK_EOF && token != TOK_ERROR);

    return token == TOK_EOF;
}

static enum JSONToken pop_token(ParsingInfo* info) {
    enum JSONToken token;
    vector_get(info->tok_list, info->idx, &token);
    info->idx++;
    return token;
}

static enum JSONToken peek_token(ParsingInfo* info) {
    enum JSONToken token;
    vector_get(info->tok_list, info->idx, &token);
    return token;
}

static TokenValue* pop_value(ParsingInfo* info) {
    TokenValue* value = vector_ref(info->tok_values, info->val_idx);
    info->val_idx++;
    return value;
}

/* === PARSING === */

static JSONNode* json_analyze(ParsingInfo* info, u32 nesting_level);

static void json_set_value(JSONNode* node, TokenValue* value) {
    switch (node->type) {
    case JSON_INT:
        json_set_int(node, value->integer);
        break;
    case JSON_FLOAT:
        json_set_double(node, value->floating);
        break;
    case JSON_STRING:
        json_set_str_direct(node, &value->str);
        break;
    case JSON_BOOL:
        json_set_bool(node, value->boolean);
        break;
    default:
        break;
    }
}

static JSONNode* json_analyze_obj(ParsingInfo* info, u32 nesting_level) {
    JSONNode* node = json_node_create(info->json, JSON_OBJECT);
    if (peek_token(info) == TOK_RBRACE) {
        pop_token(info);
        return node;
    }
    enum JSONToken token;
    do {
        token = pop_token(info);
        if (token != TOK_STRING) {
            log_errorf("JSON: Syntax error: Invalid object property type %s.", names[token]);
            if (token == TOK_RBRACE)
                log_error("JSON: Maybe there is a trailing comma ?");
            json_node_destroy(node);
            return NULL;
        }

        // Pop the colon token !
        TokenValue* val = pop_value(info);
        token = pop_token(info);
        if (token != TOK_COLON) {
            log_error("JSON: Syntax error: Missing colon after a property name.");
            return NULL;
        }

        JSONNode* subnode = json_analyze(info, nesting_level + 1);
        if (!subnode) {
            json_node_destroy(node);
            return NULL;
        }
        json_node_putn(node, &val->str, subnode);

        token = pop_token(info);
    } while (token == TOK_COMMA);

    if (token != TOK_RBRACE) {
        log_errorf("JSON: Syntax error: Expected a closing brace after property, got '%s'.",
                   names[token]);
        json_node_destroy(node);
        return NULL;
    }

    return node;
}

static JSONNode* json_analyze_array(ParsingInfo* info, u32 nesting_level) {
    JSONNode* node = json_node_create(info->json, JSON_ARRAY);
    enum JSONToken token = peek_token(info);
    if (token == TOK_RBRACKET) {
        pop_token(info);
        return node;
    }
    do {
        JSONNode* subnode = json_analyze(info, nesting_level + 1);
        if (!subnode) {
            json_node_destroy(node);
            return NULL;
        }
        json_node_addn(node, subnode);

        token = pop_token(info);
    } while (token == TOK_COMMA);

    if (token != TOK_RBRACKET) {
        log_errorf("JSON: Syntax error: Expected a closing bracket after array element, got '%s'.",
                   names[token]);
        json_node_destroy(node);
        return NULL;
    }

    return node;
}

static JSONNode* json_analyze(ParsingInfo* info, u32 nesting_level) {
    if (nesting_level >= JSON_MAX_NESTING) {
        log_error("JSON: Syntax error: Reached maximum nesting level.");
        return NULL;
    }
    enum JSONToken token = pop_token(info);

    switch (token) {
    case TOK_LBRACE:
        return json_analyze_obj(info, nesting_level);
    case TOK_LBRACKET:
        return json_analyze_array(info, nesting_level);
    case TOK_INT:
    case TOK_FLOAT:
    case TOK_BOOL:
    case TOK_STRING: {
        JSONNode* node = json_node_create(info->json, types[token]);
        json_set_value(node, pop_value(info));
        return node;
    }
    case TOK_NULL: {
        JSONNode* node = json_node_create(info->json, JSON_NULL);
        return node;
    }
    default:
        log_errorf("JSON: Syntax error: Unexpected token '%s'.", names[token]);
        return NULL;
    }
}

JSON json_parse(ByteBuffer* buffer, Arena* arena) {
    JSON json;
    json_create(&json, arena);
    Vector tok_list, tok_values;

    vector_init(&tok_list, 16, sizeof(enum JSONToken));
    vector_init(&tok_values, 4, sizeof(TokenValue));

    bool success = json_lex(buffer, &tok_list, &tok_values, arena);

    if (!success) {
        json_destroy(&json);
        goto end;
    }

    ParsingInfo parse_info = {
        .json = &json,
        .idx = 0,
        .val_idx = 0,
        .tok_list = &tok_list,
        .tok_values = &tok_values,
    };

    JSONNode* root = json_analyze(&parse_info, 0);
    if (!root) {
        json_destroy(&json);
        goto end;
    }
    json_set_root(&json, root);

end:
    vector_destroy(&tok_list);
    vector_destroy(&tok_values);

    return json;
}
