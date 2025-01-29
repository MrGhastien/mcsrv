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
#include "utils/iomux.h"
#include "utils/string.h"

#include <errno.h>
#include <string.h>
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

static bool ensure_read(IOMux fd, void* buf, u64 size) {
    while (size > 0) {
        i32 res = iomux_read(fd, buf, size);
        if (res <= 0)
            return FALSE;
        size -= res;
    }
    return TRUE;
}

static enum NBTTagType parse_type(IOMux fd, const NBTReadContext* ctx) {

    const NBTTagMetadata* parent_meta = vect_ref(&ctx->stack, ctx->stack.size - 1);

    if (!parent_meta || is_not_array(parent_meta->type)) {
        u8 type_byte;
        if (!ensure_read(fd, &type_byte, sizeof type_byte)) {
            if (iomux_eof(fd))
                log_error("NBT: Syntax error: reached unexpected end of file");
            else {
                i32 zlib_errnum;
                string zlib_error_msg = iomux_error(fd, &zlib_errnum);
                log_errorf("NBT: Syntax error: %s", cstr(&zlib_error_msg));
            }
            return _NBT_COUNT;
        }
        enum NBTTagType type = type_byte;
        if (type >= _NBT_COUNT) {
            log_errorf("NBT: Lexical error: Invalid tag type %i", type);
            return _NBT_COUNT;
        }
        return type;
    }

    const NBTTag* parent = vect_ref(&ctx->nbt->tags, parent_meta->idx);
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
    NBTTagMetadata* parent_meta = vect_ref(&ctx->stack, i);

    if (parent_meta && current_type == parent_meta->type) {
        i--;
        parent_meta = vect_ref(&ctx->stack, i);
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
        parent_meta = vect_ref(&ctx->stack, i);
        NBTTag* parent_tag = nbt_mut_ref(ctx->nbt, parent_meta->idx);

        switch (parent_meta->type) {
        case NBT_COMPOUND:
            parent_tag->data.composite.total_tag_length++;
            break;
        case NBT_LIST:
            parent_tag->data.composite.total_tag_length++;
            break;
        case NBT_BYTE_ARRAY:
        case NBT_INT_ARRAY:
        case NBT_LONG_ARRAY:
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
        const NBTTagMetadata* parent_meta = vect_ref(&ctx->stack, ctx->stack.size - 1);
        if (parent_meta->size > 0 || is_not_array(parent_meta->type))
            break;

        switch (parent_meta->type) {
        case NBT_LIST:
        case NBT_BYTE_ARRAY:
        case NBT_INT_ARRAY:
        case NBT_LONG_ARRAY:
            vect_pop(&ctx->stack, NULL);
            break;
        default:
            log_errorf("Invalid parent tag of type %i", parent_meta->type);
            return FALSE;
        }
    }
    return TRUE;
}

static bool read_string(Arena* arena, IOMux fd, string* out_str) {
    u16 name_length;
    if (!ensure_read(fd, &name_length, sizeof name_length))
        return FALSE;
    name_length = untoh16(name_length);
    *out_str = str_alloc(name_length + 1, arena);
    if (!ensure_read(fd, out_str->base, name_length))
        return FALSE;
    out_str->length = name_length;
    return TRUE;
}

static void write_string(const string* str, IOMux fd) {
    u16 clamped_length = str->length > 0xffff ? 0xffff : str->length;
    const u16 big_endian_name = uhton16(clamped_length);
    iomux_write(fd, &big_endian_name, sizeof big_endian_name);
    iomux_write(fd, str->base, clamped_length);
}

static i32 parse_array(IOMux fd, const NBTTag* new_tag, NBTReadContext* ctx) {
    i32 num;
    if (!ensure_read(fd, &num, sizeof num)) {
        int errnum;
        string errmsg = iomux_error(fd, &errnum);
        int reached_eof = iomux_eof(fd);
        if (errnum == 0 && reached_eof)
            log_error("NBT: Syntax error: Reached unexpected end of file");
        else
            log_errorf("NBT: IO Error: Failed to read the input file: %s", cstr(&errmsg));
        return -1;
    }
    num = ntoh32(num);

    if (num > 0) {
        NBTTagMetadata* new_tag_meta = vect_reserve(&ctx->stack);
        *new_tag_meta = (NBTTagMetadata){
            .size = num,
            .type = new_tag->type,
            .idx = ctx->nbt->tags.size,
        };
    }
    return num;
}

static void write_array(IOMux fd, const NBTTag* tag, NBTWriteContext* ctx) {
    i32 big_endian_size = hton32(tag->data.composite.size);
    iomux_write(fd, &big_endian_size, sizeof(i32));
    NBTTagMetadata new_parent = {
        .size = tag->data.composite.size,
        .type = tag->type,
    };
    vect_add(&ctx->stack, &new_parent);
}

static void nbt_write_tag(NBTWriteContext* ctx, IOMux fd, bool network) {
    NBTTagMetadata* parent = vect_ref(&ctx->stack, ctx->stack.size - 1);

    NBTTag* tag = vect_ref(&ctx->nbt->tags, ctx->current_index);
    if (!parent || is_not_array(parent->type)) {
        i8 type_num = tag->type & 0xff;
        iomux_write(fd, &type_num, sizeof type_num);
        if ((!network && !parent) || parent)
            write_string(&tag->name, fd);
    }
    if (parent)
        parent->size--;
    switch (tag->type) {
    case NBT_BYTE:
        iomux_write(fd, &tag->data.simple.byte, sizeof tag->data.simple.byte);
        break;
    case NBT_SHORT: {
        const i16 big_endian = hton16(tag->data.simple.short_num);
        iomux_write(fd, &big_endian, sizeof big_endian);
        break;
    }
    case NBT_FLOAT:
    case NBT_INT: {
        const i32 big_endian = hton32(tag->data.simple.integer);
        iomux_write(fd, &big_endian, sizeof big_endian);
        break;
    }
    case NBT_DOUBLE:
    case NBT_LONG: {
        const i64 big_endian = hton64(tag->data.simple.long_num);
        iomux_write(fd, &big_endian, sizeof big_endian);
        break;
    }
    case NBT_STRING: {
        write_string(&tag->data.str, fd);
        break;
    }
    case NBT_COMPOUND: {
        NBTTagMetadata new_parent = {
            .size = tag->data.composite.size,
            .type = tag->type,
        };
        vect_add(&ctx->stack, &new_parent);
        break;
    }
    case NBT_LIST: {
        iomux_write(fd, &tag->data.list.elem_type, sizeof(u8));
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

enum NBTStatus nbt_write_file(const NBT* nbt, const string* path) {

    IOMux fd = iomux_gz_open(path, "wb");
    if (fd == -1) {
        log_errorf("NBT: IO Error: %s", strerror(errno));
        return NBTE_IO;
    }

    nbt_write(nbt, fd, FALSE);

    iomux_close(fd);
    return NBTE_OK;
}

enum NBTStatus nbt_write(const NBT* nbt, IOMux multiplexer, bool network) {

    NBTWriteContext ctx = {
        .nbt = nbt,
    };
    vect_init(&ctx.stack, nbt->arena, 512, sizeof(NBTTagMetadata));
    for (u32 i = 0; i < nbt->tags.size; i++) {
        ctx.current_index = i;
        nbt_write_tag(&ctx, multiplexer, network);

        const NBTTagMetadata* parent = vect_ref(&ctx.stack, ctx.stack.size - 1);
        while (parent && parent->size == 0) {
            vect_pop(&ctx.stack, NULL);
            if (parent->type == NBT_COMPOUND) {
                const i8 end_tag = 0;
                iomux_write(multiplexer, &end_tag, sizeof end_tag);
            }
            parent = vect_ref(&ctx.stack, ctx.stack.size - 1);
        }
    }

    return NBTE_OK;
}

static bool nbt_parse_tag(IOMux fd, NBTReadContext* ctx, enum NBTTagType type) {
    NBTTagMetadata* parent_meta = vect_ref(&ctx->stack, ctx->stack.size - 1);
    NBTTag new_tag = {
        .type = type,
    };
    if ((!parent_meta || is_not_array(parent_meta->type)) && type != NBT_END) {
        if (!read_string(ctx->arena, fd, &new_tag.name))
            return FALSE;
    }

    switch (type) {
    case NBT_BYTE:
        if (!ensure_read(fd, &new_tag.data.simple.byte, sizeof new_tag.data.simple.byte))
            return FALSE;
        break;
    case NBT_SHORT: {
        i16 num;
        if (!ensure_read(fd, &num, sizeof num))
            return FALSE;
        new_tag.data.simple.short_num = ntoh16(num);
        break;
    }
    case NBT_INT: {
        i32 num;
        if (!ensure_read(fd, &num, sizeof num))
            return FALSE;
        new_tag.data.simple.integer = ntoh32(num);
        break;
    }
    case NBT_LONG: {
        i64 num;
        if (!ensure_read(fd, &num, sizeof num))
            return FALSE;
        new_tag.data.simple.long_num = ntoh64(num);
        break;
    }
    case NBT_FLOAT:
        if (!ensure_read(fd, &new_tag.data.simple.float_num, sizeof new_tag.data.simple.float_num))
            return FALSE;
        break;
    case NBT_DOUBLE:
        if (!ensure_read(
                fd, &new_tag.data.simple.double_num, sizeof new_tag.data.simple.double_num))
            return FALSE;
        break;
    case NBT_STRING: {

        if (!read_string(ctx->arena, fd, &new_tag.data.str))
            return FALSE;
        break;
    }
    case NBT_LIST: {
        u8 type_byte;
        if (!ensure_read(fd, &type_byte, sizeof type_byte))
            return FALSE;
        new_tag.data.list.elem_type = type_byte;

        i32 len = parse_array(fd, &new_tag, ctx);
        if (len == -1)
            return FALSE;
        new_tag.data.composite.size = len;
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
        NBTTagMetadata* new_tag_meta = vect_reserve(&ctx->stack);
        *new_tag_meta = (NBTTagMetadata){
            .size = 0,
            .type = type,
            .idx = ctx->nbt->tags.size,
        };
        break;
    }
    case NBT_END:
        if (!parent_meta || parent_meta->type != NBT_COMPOUND) {
            log_errorf("Unexpected end of compound tag at position %zu", 0ul);
            return FALSE;
        }
        NBTTag* parent_tag = nbt_mut_ref(ctx->nbt, parent_meta->idx);
        parent_tag->data.composite.size = parent_meta->size;
        vect_pop(&ctx->stack, NULL);
        return TRUE;
    default:
        return TRUE;
    }

    nbt_add_tag(ctx->nbt, &new_tag);

    return TRUE;
}

enum NBTStatus nbt_parse(Arena* arena, i64 max_token_count, IOMux input, NBT* out_nbt) {

    Arena parsing_arena = arena_create(600 * sizeof(NBTTagMetadata), BLK_TAG_DATA);

    NBTReadContext ctx = {
        .arena = arena,
        .nbt = out_nbt,
    };
    vect_init(&ctx.stack, &parsing_arena, 512, sizeof(NBTTagMetadata));

    nbt_init_empty(arena, max_token_count, out_nbt);

    // TODO: distinguish different errors
    do {
        const enum NBTTagType type = parse_type(input, &ctx);
        if (type == _NBT_COUNT)
            goto error_end;

        if (!nbt_parse_tag(input, &ctx, type))
            goto error_end;

        if (!update_parents(&ctx, type))
            goto error_end;

        if (!prune_parsing_stack(&ctx))
            goto error_end;

    } while (ctx.stack.size > 0);

    vect_add_imm(&out_nbt->stack, 0, i64);

    arena_destroy(&parsing_arena);
    return NBTE_OK;
error_end:
    arena_destroy(&parsing_arena);
    return NBTE_INVALID_TYPE;
}
enum NBTStatus nbt_from_file(Arena* arena, i64 max_token_count, const string* path, NBT* out_nbt) {
    IOMux fd = iomux_gz_open(path, "rb");
    if (fd == -1) {
        log_errorf("NBT: IO Error: %s", strerror(errno));
        return NBTE_IO;
    }

    enum NBTStatus status = nbt_parse(arena, max_token_count, fd, out_nbt);

    iomux_close(fd);
    return status;
}
