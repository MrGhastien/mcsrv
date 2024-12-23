//
// Created by bmorino on 20/12/2024.
//
#include "containers/vector.h"
#include "data/nbt.h"
#include "data/nbt/nbt_internal.h"
#include "definitions.h"
#include "logger.h"
#include "memory/arena.h"
#include "utils/bitwise.h"
#include "utils/string.h"

#include <zlib.h>

typedef struct NBTWriteContext {
    Vector stack;
    i32 current_index;
    const NBT* nbt;
} NBTWriteContext;

typedef struct NBTReadContext {
    Vector stack;
    Arena* arena;
    NBT* nbt;
} NBTReadContext;

typedef struct NBTTagMetadata {
    enum NBTTagType type;
    i32 size;
    i64 idx;
} NBTTagMetadata;

static enum NBTTagType parse_type(gzFile fd, const NBTReadContext* ctx) {

    const NBTTagMetadata* parent_meta = vector_ref(&ctx->stack, ctx->stack.size - 1);

    if (!parent_meta || is_list_or_array(parent_meta->type)) {
        u8 type_byte;
        gzread(fd, &type_byte, sizeof type_byte);
        enum NBTTagType type = type_byte;
        if (type >= _NBT_COUNT) {
            log_errorf("NBT: Lexical error: Invalid tag type %i", type);
            return FALSE;
        }
        return type;
    }

    const NBTTag* parent = vector_ref(&ctx->nbt->tags, parent_meta->idx);
    switch (parent_meta->type) {
    case NBT_LIST:
        return parent->data.list.elem_type;
    case NBT_BYTE_ARRAY:
        return NBT_BYTE;
    case NBT_INT_ARRAY:
        return NBT_INT;
    case NBT_LONG_ARRAY:
        return NBT_LONG;
    default:
        log_error("NBT: Syntax error: Why are we here ???");
        return _NBT_COUNT;
    }
}

static bool update_parents(NBTReadContext* ctx, enum NBTTagType current_type) {
    i64 i = (i64) ctx->stack.size - 1;
    NBTTagMetadata* parent_meta = vector_ref(&ctx->stack, i);

    if (parent_meta && current_type == parent_meta->type) {
        i--;
        parent_meta = vector_ref(&ctx->stack, i);
    }

    if (!parent_meta)
        return TRUE;

    // Update the parent tag size (only for arrays, lists, and compounds)
    // The size of compound tags starts at 0 because they are not known right away,
    // The size in the metadata gets incremented when a direct child tag is parsed.
    // when a NBT_END tag is parsed, the tag size is set to the size in the metadata,
    // and the compound metadata is removed from the parsing stack.

    // The size of lists and arrays is known when parsing the tag, so we proceed
    // in reverse.
    // When the size in the metadata reaches 0, we know we parsed all the elements,
    // and the metadata is removed from the parsing stack.

    // Increment / decrement direct parent sizes
    if (current_type != NBT_END) {
        switch (parent_meta->type) {
        case NBT_COMPOUND:
            parent_meta->size++;
            break;
        case NBT_LIST:
        case NBT_BYTE_ARRAY:
        case NBT_INT_ARRAY:
        case NBT_LONG_ARRAY:
            parent_meta->size--;
            break;
        default:
            break;
        }
    }
    // Increment the total tag length of lists and compounds.
    // Also remove metadata of tags which have all their elements parsed.
    for (i64 i = (i64) ctx->stack.size - 1; i >= 0; i--) {
        parent_meta = vector_ref(&ctx->stack, i);
        NBTTag* parent_tag = vector_ref(&ctx->nbt->tags, parent_meta->idx);

        switch (parent_meta->type) {
        case NBT_COMPOUND:
            parent_tag->data.compound.total_tag_length++;
            break;
        case NBT_LIST:
            parent_tag->data.list.total_tag_length++;
            break;
        default:
            log_errorf("Invalid parent tag of type %i", parent_meta->type);
            return FALSE;
        }
    }
    return TRUE;
}

static bool prune_parsing_stack(NBTReadContext* ctx) {
    while (ctx->stack.size > 0) {
        const NBTTagMetadata* parent_meta = vector_ref(&ctx->stack, ctx->stack.size - 1);
        if (parent_meta->size > 0 || is_list_or_array(parent_meta->type))
            break;

        switch (parent_meta->type) {
        case NBT_LIST:
        case NBT_BYTE_ARRAY:
        case NBT_INT_ARRAY:
        case NBT_LONG_ARRAY:
            vector_pop(&ctx->stack, NULL);
            break;
        default:
            log_errorf("Invalid parent tag of type %i", parent_meta->type);
            return FALSE;
        }
    }
    return TRUE;
}

static bool read_string(Arena* arena, gzFile fd, string* out_str) {
    i16 name_length;
    gzread(fd, &name_length, sizeof name_length);
    name_length = ntoh16(name_length);
    if (name_length < 0) {
        log_errorf("NBT: Syntax error: Invalid tag name length of %hi", name_length);
        return FALSE;
    }
    *out_str = str_alloc(name_length + 1, arena);
    gzread(fd, out_str->base, name_length);
    out_str->length = name_length;
    return TRUE;
}

static void write_string(const string* str, gzFile fd) {
    i16 clamped_length = str->length > 0x7fff ? 0x7fff : str->length;
    const i16 big_endian_name = hton16(clamped_length);
    gzwrite(fd, &big_endian_name, sizeof big_endian_name);
    gzwrite(fd, str->base, clamped_length);
}

static i32 parse_array(gzFile fd, NBTTag* new_tag, NBTReadContext* ctx) {
    i32 num;
    if (gzread(fd, &num, sizeof num) < 4) {
        int zlib_errnum;
        const char* zlib_error_msg = gzerror(fd, &zlib_errnum);
        int reached_eof = gzeof(fd);
        if (zlib_errnum == 0 && reached_eof)
            log_error("NBT: Syntax error: reached unexpected end of file");
        else
            log_errorf("NBT: IO Error: Failed to read the input file: %s",
                       zlib_errnum == Z_ERRNO ? strerror(errno) : zlib_error_msg);
        return -1;
    }
    num = ntoh32(num);

    if (num > 0) {
        NBTTagMetadata* new_tag_meta = vector_reserve(&ctx->stack);
        *new_tag_meta = (NBTTagMetadata){
            .size = num,
            .type = new_tag->type,
            .idx = ctx->nbt->tags.size,
        };
    }
    return num;
}

static void write_array(gzFile fd, const NBTTag* tag, NBTWriteContext* ctx) {
    i32 big_endian_size = hton32(tag->data.list.size);
    gzwrite(fd, &big_endian_size, sizeof(i32));
    NBTTagMetadata new_parent = {
        .size = tag->data.list.size,
        .type = tag->type,
    };
    vector_add(&ctx->stack, &new_parent);
}

static void nbt_write_tag(NBTWriteContext* ctx, gzFile fd) {
    NBTTagMetadata* parent = vector_ref(&ctx->stack, ctx->stack.size - 1);

    NBTTag* tag = vector_ref(&ctx->nbt->tags, ctx->current_index);
    if (!parent || is_list_or_array(parent->type)) {
        i8 type_num = tag->type & 0xff;
        gzwrite(fd, &type_num, sizeof type_num);
        write_string(&tag->name, fd);
    }
    if (parent)
        parent->size--;
    switch (tag->type) {
    case NBT_BYTE:
        gzwrite(fd, &tag->data.simple.byte, sizeof tag->data.simple.byte);
        break;
    case NBT_SHORT: {
        const i16 big_endian = hton16(tag->data.simple.short_num);
        gzwrite(fd, &big_endian, sizeof big_endian);
        break;
    }
    case NBT_FLOAT:
    case NBT_INT: {
        const i32 big_endian = hton32(tag->data.simple.integer);
        gzwrite(fd, &big_endian, sizeof big_endian);
        break;
    }
    case NBT_DOUBLE:
    case NBT_LONG: {
        const i64 big_endian = hton64(tag->data.simple.long_num);
        gzwrite(fd, &big_endian, sizeof big_endian);
        break;
    }
    case NBT_STRING: {
        write_string(&tag->data.str, fd);
        break;
    }
    case NBT_COMPOUND: {
        NBTTagMetadata new_parent = {
            .size = tag->data.compound.size,
            .type = tag->type,
        };
        vector_add(&ctx->stack, &new_parent);
        break;
    }
    case NBT_LIST: {
        gzwrite(fd, &tag->data.list.elem_type, sizeof(u8));
        write_array(fd, tag, ctx);
        break;
    }
    case NBT_BYTE_ARRAY:
    case NBT_INT_ARRAY:
    case NBT_LONG_ARRAY:
        write_array(fd, tag, ctx);
        break;
    default:
        log_errorf("NBT: Unknown NBTag type %i.", tag->type);
        break;
    }
}

void nbt_write(const NBT* nbt, const string* path) {
    NBTWriteContext ctx = {
        .nbt = nbt,
    };
    vector_init_fixed(&ctx.stack, nbt->arena, 512, sizeof(NBTTagMetadata));

    gzFile fd = gzopen(path->base, "wb");

    for (u32 i = 0; i < nbt->tags.size; i++) {
        ctx.current_index = i;
        nbt_write_tag(&ctx, fd);

        NBTTagMetadata* parent = vector_ref(&ctx.stack, ctx.stack.size - 1);
        while (parent && parent->size == 0) {
            vector_pop(&ctx.stack, NULL);
            if (parent->type == NBT_COMPOUND) {
                const i8 end_tag = 0;
                gzwrite(fd, &end_tag, sizeof end_tag);
            }
            parent = vector_ref(&ctx.stack, ctx.stack.size - 1);
        }
    }
    gzclose(fd);
}

static bool nbt_parse_tag(gzFile fd, NBTReadContext* ctx, enum NBTTagType type) {
    NBTTagMetadata* parent_meta = vector_ref(&ctx->stack, ctx->stack.size - 1);
    NBTTag new_tag = {
        .type = type,
    };
    if (!parent_meta || is_list_or_array(parent_meta->type)) {
        if (!read_string(ctx->arena, fd, &new_tag.name))
            return FALSE;
    }

    switch (type) {
    case NBT_BYTE:
        gzread(fd, &new_tag.data.simple.byte, sizeof new_tag.data.simple.byte);
        break;
    case NBT_SHORT: {
        i16 num;
        gzread(fd, &num, sizeof num);
        new_tag.data.simple.short_num = ntoh16(num);
        break;
    }
    case NBT_INT: {
        i32 num;
        gzread(fd, &num, sizeof num);
        new_tag.data.simple.integer = ntoh32(num);
        break;
    }
    case NBT_LONG: {
        i64 num;
        gzread(fd, &num, sizeof num);
        new_tag.data.simple.long_num = ntoh64(num);
        break;
    }
    case NBT_FLOAT:
        gzread(fd, &new_tag.data.simple.float_num, sizeof new_tag.data.simple.float_num);
        break;
    case NBT_DOUBLE:
        gzread(fd, &new_tag.data.simple.double_num, sizeof new_tag.data.simple.double_num);
        break;
    case NBT_STRING: {

        if (!read_string(ctx->arena, fd, &new_tag.data.str))
            return FALSE;
        break;
    }
    case NBT_LIST: {
        u8 type_byte;
        gzread(fd, &type_byte, sizeof type_byte);
        new_tag.data.list.elem_type = type_byte;

        i32 len = parse_array(fd, &new_tag, ctx);
        if (len == -1)
            return FALSE;
        new_tag.data.list.size = len;
        break;
    }
    case NBT_BYTE_ARRAY:
    case NBT_INT_ARRAY:
    case NBT_LONG_ARRAY: {
        i32 len = parse_array(fd, &new_tag, ctx);
        if (len == -1)
            return FALSE;
        new_tag.data.array_size = len;
        break;
    }
    case NBT_COMPOUND: {
        NBTTagMetadata* new_tag_meta = vector_reserve(&ctx->stack);
        *new_tag_meta = (NBTTagMetadata){
            .size = 0,
            .type = type,
            .idx = ctx->nbt->tags.size,
        };
        break;
    }
    case NBT_END:
        if (!parent_meta || parent_meta->type != NBT_COMPOUND) {
            log_errorf("Unexpected end of compound tag at position %zu", gztell(fd));
            return FALSE;
        }
        NBTTag* parent_tag = vector_ref(&ctx->nbt->tags, parent_meta->idx);
        parent_tag->data.compound.size = parent_meta->size;
        vector_pop(&ctx->stack, NULL);
        return TRUE;
    default:
        return TRUE;
    }

    vector_add(&ctx->nbt->tags, &new_tag);

    return TRUE;
}

bool nbt_parse(Arena* arena, i64 max_token_count, const string* path, NBT* out_nbt) {
    gzFile fd = gzopen(path->base, "rb");

    NBTReadContext ctx = {
        .arena = arena,
        .nbt = out_nbt,
    };
    vector_init_fixed(&ctx.stack, arena, 512, sizeof(NBTTagMetadata));

    *out_nbt = nbt_create(arena, max_token_count);
    out_nbt->tags.size = 0;
    out_nbt->stack.size = 0;

    do {
        const enum NBTTagType type = parse_type(fd, &ctx);

        if (!nbt_parse_tag(fd, &ctx, type))
            return FALSE;

        if (!update_parents(&ctx, type))
            return FALSE;

        if (!prune_parsing_stack(&ctx))
            return FALSE;

    } while (ctx.stack.size > 0);

    gzclose(fd);
    return TRUE;
}
