#include "containers/vector.h"
#include "data/json.h"
#include "definitions.h"
#include "json_internal.h"
#include "logger.h"
#include "memory/arena.h"
#include "memory/mem_tags.h"
#include "utils/iomux.h"
#include "utils/str_builder.h"
#include "utils/string.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define JSON_MAX_NESTING 32

typedef struct JSONWriteContext {
    Vector stack;
    i32 current_index;
    const JSON* json;
} JSONWriteContext;

typedef struct JSONReadContext {
    Vector stack;
    Arena* arena;
    JSON* json;
} JSONReadContext;

typedef struct JSONTokenMetadata {
    enum JSONType type;
    i32 size;
    i64 idx;
} JSONTokenMetadata;

static void add_new_parent(JSONWriteContext* ctx, JSONToken* token) {
    JSONTokenMetadata new_parent = {
        .size = token->data.compound.size,
        .type = token->type,
        .idx = ctx->current_index,
    };
    vect_add(&ctx->stack, &new_parent);
}

static void write_indent(JSONWriteContext* ctx, IOMux fd) {
    u64 indent = vect_size(&ctx->stack);
    for (u64 i = 0; i < indent; i++) {
        iomux_writes(fd, "    ");
    }
}

static void json_write_token(JSONWriteContext* ctx, IOMux fd) {
    JSONTokenMetadata* parent = vect_ref(&ctx->stack, ctx->stack.size - 1);

    JSONToken* token = vect_ref(&ctx->json->tokens, ctx->current_index);

    write_indent(ctx, fd);

    if (parent && parent->type == JSON_OBJECT) {
        iomux_writec(fd, '"');
        iomux_write_str(fd, &token->name);
        iomux_writes(fd, "\": ");
    }

    switch (token->type) {
    case JSON_BOOL:
        iomux_writes(fd, token->data.simple.boolean ? "true" : "false");
        break;
    case JSON_NULL:
        iomux_writes(fd, "null");
        break;
    case JSON_INT:
        iomux_writef(fd, "%lli", token->data.simple.number);
        break;
    case JSON_FLOAT:
        iomux_writef(fd, "%llf", token->data.simple.fnumber);
        break;
    case JSON_ARRAY:
        iomux_writec(fd, '[');
        if (token->data.compound.size == 0)
            iomux_writec(fd, ']');
        else
            add_new_parent(ctx, token);
        break;
    case JSON_OBJECT:
        iomux_writec(fd, '{');
        if (token->data.compound.size == 0)
            iomux_writec(fd, '}');
        else
            add_new_parent(ctx, token);
        break;
    case JSON_STRING:
        iomux_writec(fd, '"');
        iomux_write_str(fd, &token->data.str);
        iomux_writec(fd, '"');
        break;
    default:
        log_errorf("NBT: Unknown JSON token type %i.", token->type);
        break;
    }
}

enum JSONStatus json_write(const JSON* json, IOMux multiplexer) {

    JSONWriteContext ctx = {
        .json = json,
    };
    vect_init(&ctx.stack, json->arena, 512, sizeof(JSONTokenMetadata));

    u32 token_count = vect_size(&json->tokens);
    for (u32 i = 0; i < token_count; i++) {
        ctx.current_index = i;
        json_write_token(&ctx, multiplexer);

        JSONTokenMetadata* parent;
        while ((parent = vect_ref(&ctx.stack, vect_size(&ctx.stack) - 1)) && parent->idx != i) {
            parent->size--;
            if (parent->size > 0) {
                iomux_writec(multiplexer, ',');
                break;
            }

            iomux_writec(multiplexer, '\n');
            vect_pop(&ctx.stack, NULL);
            write_indent(&ctx, multiplexer);
            if (parent->type == JSON_ARRAY)
                iomux_writec(multiplexer, ']');
            else if (parent->type == JSON_OBJECT)
                iomux_writec(multiplexer, '}');
        }
        iomux_writec(multiplexer, '\n');
    }
    return JSONE_OK;
}

enum JSONStatus json_write_file(const JSON* json, const string* path);

enum JSONStatus json_to_string(const JSON* json, Arena* arena, string* out_str) {
    IOMux mux = iomux_new_string(arena);

    json_write(json, mux);

    *out_str = iomux_string(mux, arena);

    iomux_close(mux);
    return JSONE_OK;
}

/* ===== PARSING ===== */
enum JSONLexUnit {
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

typedef union unit_value {
    bool boolean;
    i64 integer;
    f64 floating;
    string str;
} LexUnitValue;

typedef struct parse_info {
    Vector* tok_list;
    Vector* tok_values;
    u64 idx;
    u64 val_idx;
    JSON* json;
    string name;
} ParsingInfo;

static char* names[_TOK_COUNT] = {
    [TOK_LBRACE] = "{",
    [TOK_RBRACE] = "}",
    [TOK_LBRACKET] = "[",
    [TOK_RBRACKET] = "]",
    [TOK_COMMA] = ",",
    [TOK_COLON] = ":",
    [TOK_NULL] = "null",
    [TOK_ERROR] = "<ERROR>",
    [TOK_EOF] = "<EOF>",
    [TOK_STRING] = "<string>",
    [TOK_INT] = "<integer>",
    [TOK_FLOAT] = "<float>",
    [TOK_BOOL] = "<bool>",
};

static enum JSONType types[_TOK_COUNT] = {
    [TOK_RBRACE] = _JSON_COUNT,
    [TOK_RBRACKET] = _JSON_COUNT,
    [TOK_COMMA] = _JSON_COUNT,
    [TOK_COLON] = _JSON_COUNT,
    [TOK_ERROR] = _JSON_COUNT,
    [TOK_EOF] = _JSON_COUNT,

    [TOK_LBRACE] = JSON_OBJECT,
    [TOK_LBRACKET] = JSON_ARRAY,
    [TOK_NULL] = JSON_NULL,
    [TOK_STRING] = JSON_STRING,
    [TOK_INT] = JSON_INT,
    [TOK_FLOAT] = JSON_FLOAT,
    [TOK_BOOL] = JSON_BOOL,
};

static bool token_has_value(enum JSONLexUnit token) {
    return token >= TOK_STRING;
}

static bool lex_string(IOMux multiplexer, LexUnitValue* value, Arena* arena) {
    char c;
    Arena scratch = *arena;
    StringBuilder builder = strbuild_create(&scratch);
    while (iomux_read(multiplexer, &c, 1) == 1 && c != '"') {
        strbuild_appendc(&builder, c);
        if (c == '\\') {
            iomux_read(multiplexer, NULL, 1);
            strbuild_appendc(&builder, c);
        }
    }
    if (c != '"')
        return FALSE;

    value->str = strbuild_to_string(&builder, arena);

    return TRUE;
}

static enum JSONLexUnit
lex_number(IOMux multiplexer, char first, LexUnitValue* value, Arena* arena) {
    bool frac = FALSE;
    bool exponent = FALSE;

    Arena scratch = *arena;
    StringBuilder builder = strbuild_create(&scratch);
    strbuild_appendc(&builder, first);
    char c;
    bool stop = FALSE;
    while (!stop && !iomux_eof(multiplexer)) {
        do {
            if (iomux_read(multiplexer, &c, 1) <= 0)
                return TOK_ERROR;
            strbuild_appendc(&builder, c);
        } while (c >= '0' && c <= '9');

        switch (c) {
        case '.':
            if (frac)
                return TOK_ERROR;
            frac = TRUE;
            break;
        case 'e':
        case 'E':
            if (exponent)
                return TOK_ERROR;
            exponent = TRUE;
            break;
        default:
            stop = TRUE;
            break;
        }
    }

    string parse_res = strbuild_to_string(&builder, &scratch);

    enum JSONLexUnit token;
    if (frac) {
        value->floating = strtod(cstr(&parse_res), NULL);
        token = TOK_FLOAT;
    } else {
        value->integer = strtol(cstr(&parse_res), NULL, 10);
        token = TOK_INT;
    }

    iomux_ungetc(multiplexer, c);
    return token;
}

static bool lex_boolean(IOMux multiplexer, LexUnitValue* value, bool expected) {
    u8 tmp[5];
    if (expected) {
        if (iomux_read(multiplexer, tmp, 3) != 3)
            return FALSE;
        if (memcmp(tmp, "rue", 3) != 0)
            return FALSE;
        value->boolean = TRUE;
    } else {
        if (iomux_read(multiplexer, tmp, 4) != 4)
            return FALSE;
        if (memcmp(tmp, "alse", 4) != 0)
            return FALSE;
        value->boolean = FALSE;
    }
    return TRUE;
}

static bool lex_null(IOMux multiplexer) {
    u8 tmp[3];
    if (iomux_read(multiplexer, tmp, sizeof(tmp)) != 3)
        return FALSE;
    return memcmp(tmp, "ull", 3) == 0;
}

static enum JSONLexUnit lex_token(IOMux multiplexer, LexUnitValue* value, Arena* arena) {
    char c;
    do {
        if (iomux_read(multiplexer, &c, 1) < 1)
            return TOK_EOF;
    } while (isspace(c));

    switch (c) {
    case '{':
        return TOK_LBRACE;
    case '}':
        return TOK_RBRACE;
    case '[':
        return TOK_LBRACKET;
    case ']':
        return TOK_RBRACKET;
    case ',':
        return TOK_COMMA;
    case ':':
        return TOK_COLON;
    case '"':
        if (lex_string(multiplexer, value, arena))
            return TOK_STRING;
        else
            return TOK_ERROR;
    case 't':
        if (lex_boolean(multiplexer, value, TRUE))
            return TOK_BOOL;
        else
            return TOK_ERROR;
    case 'f':
        if (lex_boolean(multiplexer, value, FALSE))
            return TOK_BOOL;
        else
            return TOK_ERROR;
    case 'n':
        return lex_null(multiplexer) ? TOK_NULL : TOK_ERROR;
    case 0:
        return TOK_EOF;
    default:
        if ((c >= '0' && c <= '9') || c == '-')
            return lex_number(multiplexer, c, value, arena);
        log_errorf("JSON: Lexical error: Unknown character '%c'.", c);
        return TOK_ERROR;
    }
}

static bool json_lex(IOMux multiplexer, Vector* tok_list, Vector* tok_values, Arena* arena) {
    enum JSONLexUnit token;
    LexUnitValue value;
    do {
        token = lex_token(multiplexer, &value, arena);
        vect_add(tok_list, &token);
        if (token_has_value(token))
            vect_add(tok_values, &value);
    } while (token != TOK_EOF && token != TOK_ERROR);

    return token == TOK_EOF;
}

static enum JSONLexUnit peek_token(ParsingInfo* info) {
    enum JSONLexUnit unit;
    vect_get(info->tok_list, info->idx, &unit);
    if (unit >= _TOK_COUNT || unit < TOK_LBRACE)
        log_errorf("Tokenized invalid lexical unit %i", unit);
    return unit;
}

static enum JSONLexUnit pop_token(ParsingInfo* info) {
    enum JSONLexUnit unit;
    vect_get(info->tok_list, info->idx, &unit);
    if (unit >= _TOK_COUNT || unit < TOK_LBRACE)
        log_errorf("Tokenized invalid lexical unit %i", unit);
    info->idx++;
    return unit;
}

static LexUnitValue* pop_value(ParsingInfo* info) {
    LexUnitValue* value = vect_ref(info->tok_values, info->val_idx);
    info->val_idx++;
    return value;
}

static void add_token(ParsingInfo* info, JSONToken* new_token) {
    JSONToken* parent = vect_ref(&info->json->tokens, new_token->parent_index);
    increment_parent_total_lengths(info->json);
    if (parent)
        parent->data.compound.size++;
    vect_add(&info->json->tokens, new_token);
}

static void json_assign_value(JSONToken* token, LexUnitValue* value) {
    switch (token->type) {
    case JSON_INT:
        token->data.simple.number = value->integer;
        break;
    case JSON_FLOAT:
        token->data.simple.fnumber = value->floating;
        break;
    case JSON_BOOL:
        token->data.simple.boolean = value->boolean;
        break;
    case JSON_STRING:
        token->data.str = value->str;
        break;
    default:
        break;
    }
}

static enum JSONStatus json_analyze(ParsingInfo* info);

static enum JSONStatus json_analyze_obj(ParsingInfo* info) {
    enum JSONLexUnit unit = peek_token(info);
    if (unit == TOK_RBRACE) {
        pop_token(info);
        return JSONE_OK;
    }
    LexUnitValue* val;
    do {
        unit = pop_token(info);
        if (unit != TOK_STRING) {
            log_errorf("Invalid property type: expected string, got '%s'.", names[unit]);
            return JSONE_INVALID_TYPE;
        }
        val = pop_value(info);
        unit = pop_token(info);
        if (unit != TOK_COLON) {
            log_errorf("Invalid property delimiter: expected ':', got '%s'.", names[unit]);
            return JSONE_UNEXPECTED_TOKEN;
        }

        // NOTE : This works only because the block containing characters is not part of
        // the string structure !
        info->name = val->str;
        enum JSONStatus status = json_analyze(info);
        if (status != JSONE_OK)
            return status;
        unit = pop_token(info);
    } while (unit == TOK_COMMA);
    if (unit != TOK_RBRACE) {
        log_errorf("Unexpected token: expected '}', got '%s'.", names[unit]);
        log_error("Perhaps a comma is missing ?");
        return JSONE_UNEXPECTED_TOKEN;
    }

    return JSONE_OK;
}

static enum JSONStatus json_analyze_array(ParsingInfo* info) {
    enum JSONLexUnit unit = peek_token(info);
    if (unit == TOK_RBRACKET) {
        pop_token(info);
        return JSONE_OK;
    }
    do {
        enum JSONStatus status = json_analyze(info);
        if (status != JSONE_OK)
            return status;
        unit = pop_token(info);
    } while (unit == TOK_COMMA);
    if (unit != TOK_RBRACKET) {
        log_errorf("Unexpected token: expected ']', got '%s'.", names[unit]);
        log_error("Perhaps a comma is missing ?");
        return JSONE_UNEXPECTED_TOKEN;
    }
    return JSONE_OK;
}

static enum JSONStatus json_analyze(ParsingInfo* info) {
    if(vect_size(&info->json->stack) > 512) {
        log_error("Reached maximum nesting depth of 512");
        return JSONE_MAX_NESTING;
    }
    enum JSONStatus status;
    enum JSONLexUnit unit = pop_token(info);
    LexUnitValue* val = NULL;
    i64 parent_index = -1;
    vect_get(&info->json->stack, vect_size(&info->json->stack) - 1, &parent_index);

    JSONToken new_token = {
        .name = info->name,
        .parent_index = parent_index,
        .type = types[unit],
    };

    switch (unit) {
    case TOK_LBRACE: {
        i64 new_index = vect_size(&info->json->tokens);
        add_token(info, &new_token);
        if(!vect_add(&info->json->stack, &new_index)) {
            log_error("Reached maximum nesting depth of 512");
            return JSONE_MAX_NESTING;
        }
        status = json_analyze_obj(info);
        vect_pop(&info->json->stack, NULL);
        return status;
    }
    case TOK_LBRACKET: {
        i64 new_index = vect_size(&info->json->tokens);
        add_token(info, &new_token);
        if(!vect_add(&info->json->stack, &new_index)) {
            log_error("Reached maximum nesting depth of 512");
            return JSONE_MAX_NESTING;
        }
        status = json_analyze_array(info);
        vect_pop(&info->json->stack, NULL);
        return status;
    }

    case TOK_INT:
    case TOK_FLOAT:
    case TOK_BOOL:
    case TOK_STRING:
        val = pop_value(info);
        json_assign_value(&new_token, val);
        EXPLICIT_FALLTHROUGH;
    case TOK_NULL:
        add_token(info, &new_token);
        return JSONE_OK;
    default:
        log_errorf("Unexpected token %s.", names[unit]);
        return JSONE_UNEXPECTED_TOKEN;
    }
}

enum JSONStatus json_parse(IOMux multiplexer, Arena* arena, JSON* out_json) {
    Vector units;
    Vector unit_values;

    Arena parsing_arena = arena_create(1 << 16, BLK_TAG_DATA);

    vect_init_dynamic(&units, &parsing_arena, 16, sizeof(enum JSONLexUnit));
    vect_init_dynamic(&unit_values, &parsing_arena, 4, sizeof(LexUnitValue));

    /* There is no need for the parsing arena here, because :
       - numbers use a scratch arena (made by copying the JSON arena into a stack variable)
       - allocated buffers of strings will be used as-is in the json tree.
       - Other tokens do not require dynamic memory allocations.
    */
    bool res = json_lex(multiplexer, &units, &unit_values, arena);
    if (!res) {
        arena_destroy(&parsing_arena);
        return JSONE_UNKNOWN_TOKEN;
    }

    ParsingInfo info = {
        .json = out_json,
        .idx = 0,
        .val_idx = 0,
        .tok_list = &units,
        .tok_values = &unit_values,
        .name = STR_EMPTY,
    };

    *out_json = json_create(arena, 64);

    enum JSONStatus status = json_analyze(&info);
    if (status == JSONE_OK) {
        enum JSONLexUnit unit = pop_token(&info);
        if (unit != TOK_EOF) {
            log_error("Extra tokens detected after end of JSON tree");
            status = JSONE_UNEXPECTED_TOKEN;
        } else if (vect_size(&info.json->stack) > 0) {
            log_error("Missing delimiter for composite token.");
            status = JSONE_MISSING_TOKEN;
        } else {
            assert(info.val_idx == vect_size(&unit_values));
            vect_add_imm(&info.json->stack, 0, i64);
        }
    }
    arena_destroy(&parsing_arena);
    return status;
}
