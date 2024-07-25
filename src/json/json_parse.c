#include "json.h"

#include "json_internal.h"
#include "containers/vector.h"
#include "logger.h"
#include "memory/arena.h"
#include "utils/bitwise.h"
#include "utils/string.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

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

static bool token_has_value(enum JSONToken token) {
    return token >= TOK_STRING;
}

static bool lex_string(ByteBuffer* buffer, TokenValue* value, Arena* arena) {
    u64 start = buffer->read_head;
    char c;
    while (bytebuf_read(buffer, 1, &c) == 1 && c != '"') {
        if (c == '\\') {
            bytebuf_read(buffer, 1, NULL);
        }
    }
    if (c != '"')
        return FALSE;

    value->str =
        str_create_from_buffer(offset(buffer->buf, start), buffer->read_head - start, arena);

    return TRUE;
}

static bool is_valid_number_char(char c, bool frac, bool exponent) {
    return (c >= '0' && c <= '9') || (!frac && c == '.') || (!exponent && (c == 'e' || c == 'E'));
}

static enum JSONToken lex_number(ByteBuffer* buffer, TokenValue* value, Arena* arena) {
    bool frac = FALSE;
    bool exponent = FALSE;

    u64 start = buffer->read_head;
    char c;
    while (bytebuf_peek(buffer, 1, &c) == 1 && is_valid_number_char(c, frac, exponent)) {
        bytebuf_read(buffer, 1, NULL);
    }
    if (c == '.' && frac)
        return TOK_ERROR;
    if ((c == 'e' || c == 'E') && exponent)
        return TOK_ERROR;

    arena_save(arena);
    string str =
        str_create_from_buffer(offset(buffer->buf, start), buffer->read_head - start, arena);

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

static enum JSONToken lex_token(ByteBuffer* buffer, TokenValue* value, Arena* arena) {
    char c;
    do {
    if (bytebuf_read(buffer, 1, &c) < 1)
        return TOK_EOF;
    } while(isspace(c));

    switch (c) {
    case '{': return TOK_LBRACE;
    case '}': return TOK_RBRACE;
    case '[': return TOK_LBRACKET;
    case ']': return TOK_RBRACKET;
    case ',': return TOK_COMMA;
    case ':': return TOK_COLON;
    case '"':
        if (lex_string(buffer, value, arena))
            return TOK_STRING;
        else
            return TOK_ERROR;
    case 0: return TOK_EOF;
    default:
        if ((c >= '0' && c <= '9') || c == '-')
            return lex_number(buffer, value, arena);
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

static bool json_analyze(JSONNode* parent, ParsingInfo* info);

static void json_set_value(JSONNode* node, TokenValue* value) {
    switch(node->type) {
    case JSON_INT:
        json_set_int(node, value->integer);
        break;
    case JSON_FLOAT:
        json_set_double(node, value->floating);
        break;
    case JSON_STRING:
        json_set_str(node, &value->str);
        break;
    case JSON_BOOL:
        json_set_bool(node, value->boolean);
        break;
    default:
        break;
    }
}

static bool json_analyze_obj(JSONNode* parent, ParsingInfo* info) {
    enum JSONToken token;
    do {
        token = pop_token(info);
        if(token == TOK_RBRACE)
            break;
        if(token != TOK_STRING) {
            log_errorf("JSON: Parsing error: Invalid object property type %s.", names[token]);
            return FALSE;
        }

        TokenValue* val = pop_value(info);
        token = peek_token(info);
        enum JSONType type = types[token];
        if(type == _JSON_COUNT) {
            log_errorf("JSON: Parsing error: Invalid token for property value '%s'.", names[token]);
            return FALSE;
        }
        JSONNode* subnode = json_node_puts(info->json, parent, &val->str, type);
        json_analyze(subnode, info);
            
    } while(token != TOK_RBRACE);

    return TRUE;
}

static bool json_analyze_array(JSONNode* parent, ParsingInfo* info) {
    enum JSONToken token;
    do {
        token = pop_token(info);
        if(token == TOK_RBRACKET)
            break;

        token = peek_token(info);
        enum JSONType type = types[token];
        if(type == _JSON_COUNT) {
            log_errorf("JSON: Parsing error: Invalid token for array element '%s'.", names[token]);
            return FALSE;
        }
        JSONNode* subnode = json_node_add(info->json, parent, type);
        json_analyze(subnode, info);
            
    } while(token != TOK_RBRACE);

    return TRUE;
}

static bool json_analyze(JSONNode* parent, ParsingInfo* info) {
    enum JSONToken token = pop_token(info);

    switch(token) {
    case TOK_LBRACE:
        return json_analyze_obj(parent, info);
    case TOK_LBRACKET:
        return json_analyze_array(parent, info);
    case TOK_INT:
    case TOK_FLOAT:
    case TOK_BOOL:
    case TOK_STRING:
        json_set_value(parent, pop_value(info));
        return TRUE;
    default:
        return TRUE;
    }
}

JSON json_parse(ByteBuffer* buffer, Arena* arena) {
    JSON json;
    json_create(&json, arena);
    Vector tok_list, tok_values;

    vector_init(&tok_list, 16, sizeof(enum JSONToken));
    vector_init(&tok_values, 4, sizeof(TokenValue));

    bool success = json_lex(buffer, &tok_list, &tok_values, arena);

    for (u64 i = 0; i < tok_list.size; i++) {
        enum JSONToken* token = vector_ref(&tok_list, i);
        printf("%s ", names[*token]);
    }

    if(!success) {
        log_error("JSON: Lexing error.");
        json_destroy(&json);
        return json;
    }

    ParsingInfo parse_info = {
        .json = &json,
        .idx = 0,
        .val_idx = 0,
        .tok_list = &tok_list,
        .tok_values = &tok_values,
    };

    success = json_analyze(json.root, &parse_info);
    if(!success) {
        log_error("JSON: Parsing error.");
        json_destroy(&json);
        return json;
    }

    vector_destroy(&tok_list);
    vector_destroy(&tok_values);

    return json;
}
