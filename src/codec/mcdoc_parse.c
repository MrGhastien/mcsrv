#include "containers/vector.h"
#include "definitions.h"
#include "logger.h"
#include "memory/_memory_internal.h"
#include "memory/allocators/arena.h"
#include "memory/mem_tags.h"
#include "network/packet.h"
#include "resource/resource_id.h"
#include "utils/iomux.h"
#include "utils/str_builder.h"
#include "utils/string.h"
#include <ctype.h>
#include <openssl/bio.h>

enum McdocTokenType {
    _MCDOC_TOK_NONE = 0,
    MCDOC_TOK_INTEGER,
    MCDOC_TOK_FLOAT,
    MCDOC_TOK_STRING,
    MCDOC_TOK_RESID,
    MCDOC_TOK_IDENTIFIER,

    MCDOC_TOK_LPAREN,
    MCDOC_TOK_RPAREN,
    MCDOC_TOK_LBRACE,
    MCDOC_TOK_RBRACE,
    MCDOC_TOK_LBRACKET,
    MCDOC_TOK_RBRACKET,
    MCDOC_TOK_LCHEVRON,
    MCDOC_TOK_RCHEVRON,
    MCDOC_TOK_COMMA,
    MCDOC_TOK_COLON,
    MCDOC_TOK_PIPE,
    MCDOC_TOK_QUESTION,
    MCDOC_TOK_AT,
    MCDOC_TOK_DOUBLE_COLON,
    MCDOC_TOK_EQ,
    MCDOC_TOK_DOT,
    MCDOC_TOK_RANGE,

    MCDOC_TOK_KW_ANY,
    MCDOC_TOK_KW_BYTE,
    MCDOC_TOK_KW_SHORT,
    MCDOC_TOK_KW_INT,
    MCDOC_TOK_KW_LONG,
    MCDOC_TOK_KW_FLOAT,
    MCDOC_TOK_KW_DOUBLE,
    MCDOC_TOK_KW_STRING,
    MCDOC_TOK_KW_FALSE,
    MCDOC_TOK_KW_TRUE,
    MCDOC_TOK_KW_BOOLEAN,
    MCDOC_TOK_KW_ENUM,
    MCDOC_TOK_KW_STRUCT,
    MCDOC_TOK_KW_FALLBACK,
    MCDOC_TOK_KW_NONE,
    MCDOC_TOK_KW_UNKNOWN,
    MCDOC_TOK_KW_KEY,
    MCDOC_TOK_KW_PARENT,
    MCDOC_TOK_KW_TYPE,
    MCDOC_TOK_KW_USE,
    MCDOC_TOK_KW_AS,
    MCDOC_TOK_KW_INJECT,
    MCDOC_TOK_KW_DISPATCH,
    MCDOC_TOK_KW_TO,
    MCDOC_TOK_KW_SUPER,
};

enum NumberType {
    NUM_BYTE,
    NUM_SHORT,
    NUM_INT,
    NUM_LONG,
    NUM_FLOAT,
    NUM_DOUBLE,
};
union Number {
    i8 i8;
    i16 i16;
    i32 i32;
    i64 i64;
    f32 f32;
    f64 f64;
};

typedef struct {
    enum McdocTokenType type;
    union {
        struct {
            enum NumberType type;
            union Number data;
        } number;

        /** Stores strings, identifiers, reserved and keywords. */
        string str;
        struct {
            enum NumberType type;
            bool has_start;
            bool has_end;
            union Number start;
            union Number end;
        } range;
        ResourceID resid;
        struct {
            string* elements;
            u64 count;
        } path;
    } data;
    i32 line;
} McdocToken;

typedef struct {
    Vector tokens;
    i32 line;
    bool has_error;
    Arena arena;
} ParseCtx;

struct keyword_mapping {
    string keyword;
    enum McdocTokenType type;
};

#define MAP_KEYWORD(cstr, name)                                                                    \
    [MCDOC_TOK_KW_##name -                                                                         \
        MCDOC_TOK_KW_ANY] = {.keyword = STR_STATIC(cstr), .type = MCDOC_TOK_KW_##name}
static const struct keyword_mapping keyword_map[] = {
    MAP_KEYWORD("any", ANY),
    MAP_KEYWORD("byte", BYTE),
    MAP_KEYWORD("short", SHORT),
    MAP_KEYWORD("int", INT),
    MAP_KEYWORD("long", LONG),
    MAP_KEYWORD("float", FLOAT),
    MAP_KEYWORD("double", DOUBLE),
    MAP_KEYWORD("string", STRING),
    MAP_KEYWORD("false", FALSE),
    MAP_KEYWORD("true", TRUE),
    MAP_KEYWORD("boolean", BOOLEAN),
    MAP_KEYWORD("enum", ENUM),
    MAP_KEYWORD("struct", STRUCT),
    MAP_KEYWORD("fallback", FALLBACK),
    MAP_KEYWORD("none", NONE),
    MAP_KEYWORD("unknown", UNKNOWN),
    MAP_KEYWORD("key", KEY),
    MAP_KEYWORD("parent", PARENT),
    MAP_KEYWORD("type", TYPE),
    MAP_KEYWORD("use", USE),
    MAP_KEYWORD("as", AS),
    MAP_KEYWORD("inject", INJECT),
    MAP_KEYWORD("dispatch", DISPATCH),
    MAP_KEYWORD("to", TO),
    MAP_KEYWORD("super", SUPER),
};

static void add_token(ParseCtx* ctx, enum McdocTokenType type) {
    McdocToken tok = {.type = type, .line = ctx->line};
    vect_add(&ctx->tokens, &tok);
}

static void add_general_token(ParseCtx* ctx, const McdocToken token) {
    vect_add(&ctx->tokens, &token);
}

static void lex_number(ParseCtx* ctx, IOMux input) {
    UNUSED(ctx);
    UNUSED(input);
}

static void lex_string(ParseCtx* ctx, IOMux input) {
    char c;
    Arena arena = arena_create(4096, BLK_TAG_DATA, INVALID_CHAIN);
    StringBuilder builder = strbuild_create(&arena);

    while ((c = iomux_getc(input)) >= 0 && c != '"') {
        char d = iomux_getc(input);
        if (d == '\n')
            ctx->line++;
        else
            iomux_ungetc(input, d);
        strbuild_appendc(&builder, c);
    }

    if (iomux_eof(input)) {
        log_errorf("Unterminated string at line %i", ctx->line);
        ctx->has_error = TRUE;
    }

    add_general_token(ctx,
                      (McdocToken) {
                          .type     = MCDOC_TOK_STRING,
                          .data.str = strbuild_to_string(&builder, &ctx->arena),
                      });
    arena_destroy(&arena);
}

static void lex_token(ParseCtx* ctx, IOMux input) {
    UNUSED(keyword_map);
    char c = iomux_getc(input);

    switch (c) {
    case '(': add_token(ctx, MCDOC_TOK_LPAREN); break;
    case ')': add_token(ctx, MCDOC_TOK_RPAREN); break;
    case '{': add_token(ctx, MCDOC_TOK_LBRACE); break;
    case '}': add_token(ctx, MCDOC_TOK_RBRACE); break;
    case '[': add_token(ctx, MCDOC_TOK_LBRACKET); break;
    case ']': add_token(ctx, MCDOC_TOK_RBRACKET); break;
    case '<': add_token(ctx, MCDOC_TOK_LCHEVRON); break;
    case '>': add_token(ctx, MCDOC_TOK_RCHEVRON); break;
    case ',': add_token(ctx, MCDOC_TOK_COMMA); break;
    case '|': add_token(ctx, MCDOC_TOK_PIPE); break;
    case '?': add_token(ctx, MCDOC_TOK_QUESTION); break;
    case '@': add_token(ctx, MCDOC_TOK_AT); break;
    case '=': add_token(ctx, MCDOC_TOK_EQ); break;
    case '.': {
        char d = iomux_getc(input);
        if(d == '.')
            add_token(ctx, MCDOC_TOK_RANGE);
        else if (isdigit(d))
            lex_number(ctx, input);
        else {
            iomux_ungetc(input, d);
            add_token(ctx, MCDOC_TOK_DOT);
        }
        break;
    }
    case ':': {
        char d = iomux_getc(input);
        add_token(ctx, d == ':' ? MCDOC_TOK_DOUBLE_COLON : MCDOC_TOK_COLON);
        break;
    }
    case '"': lex_string(ctx, input); break;
    default:
        ctx->has_error = TRUE;
        log_errorf("Unexpected character at line %i: '%c'.", ctx->line, c);
        break;
    }
}

static void mcdoc_tokenize(ParseCtx* ctx, IOMux input) {
    ctx->line       = 1;

    i32 chr;
    while ((chr = iomux_getc(input))) {
        lex_token(ctx, input);
    }
}

void mcdoc_parse(const string filename) {
    Arena parse_arena = arena_create(1 << 20, BLK_TAG_DATA, INVALID_CHAIN);

    ParseCtx ctx;
    vect_init_dynamic(&ctx.tokens, &parse_arena, 64, sizeof(McdocToken));
    IOMux input = iomux_open(&filename, "r");

    mcdoc_tokenize(&ctx, input);

    iomux_close(input);
    arena_destroy(&parse_arena);
}
