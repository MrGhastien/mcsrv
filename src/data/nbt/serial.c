//
// Created by bmorino on 20/12/2024.
//
#include "containers/vector.h"
#include "data/nbt.h"
#include "data/nbt/nbt_types.h"
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

typedef struct NBTWriteTag {
    enum NBTTagType type;
    i32 size;
} NBTWriteTag;

typedef struct NBTReadContext {
    Vector stack;
    Arena* arena;
    NBT* nbt;
} NBTReadContext;

typedef struct NBTReadTag {
    enum NBTTagType type;
    i32 size;
    i64 idx;
} NBTReadTag;

static void nbt_write_tag(NBTWriteContext* ctx, gzFile fd) {
    NBTWriteTag* parent = vector_ref(&ctx->stack, ctx->stack.size - 1);

    NBTTag* tag = vector_ref(&ctx->nbt->tags, ctx->current_index);
    tag->name = (string){0};
    tag->data = (union NBTValue){0};
    if (!parent || parent->type != NBT_LIST) {
        i8 type_num = tag->type & 0xff;
        gzwrite(fd, &type_num, sizeof type_num);
        const i16 big_endian_name = hton16(tag->name.length & 0xffff);
        gzwrite(fd, &big_endian_name, sizeof big_endian_name);
        gzwrite(fd, tag->name.base, tag->name.length);
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
        const i32 big_endian = hton32(tag->data.str.length & 0xffff);
        gzwrite(fd, &big_endian, sizeof big_endian);
        gzwrite(fd, tag->data.str.base, tag->data.str.length);
        break;
    }
    case NBT_COMPOUND: {
        NBTWriteTag new_parent = {
            .size = tag->data.compound.size,
            .type = tag->type,
        };
        vector_add(&ctx->stack, &new_parent);
        break;
    }
    case NBT_LIST: {
        gzwrite(fd, &tag->data.list.elem_type, sizeof(u8));
        i32 big_endian_size = hton32(tag->data.list.size);
        gzwrite(fd, &big_endian_size, sizeof(i32));
        NBTWriteTag new_parent = {
            .size = tag->data.list.size,
            .type = tag->type,
        };
        vector_add(&ctx->stack, &new_parent);
        break;
    }
    case NBT_BYTE_ARRAY:
    case NBT_INT_ARRAY:
    case NBT_LONG_ARRAY:
        gzwrite(fd, &tag->data.array_size, sizeof(i32));
        NBTWriteTag new_parent = {
            .size = tag->data.array_size,
            .type = tag->type,
        };
        vector_add(&ctx->stack, &new_parent);
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
    vector_init_fixed(&ctx.stack, nbt->arena, 512, sizeof(NBTWriteTag));

    gzFile fd = gzopen(path->base, "wb");

    for (u32 i = 0; i < nbt->tags.size; i++) {
        ctx.current_index = i;
        nbt_write_tag(&ctx, fd);

        NBTWriteTag* parent = vector_ref(&ctx.stack, ctx.stack.size - 1);
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
    NBTReadTag* parent = vector_ref(&ctx->stack, ctx->stack.size - 1);
    NBTTag tag = {
        .type = type,
    };
    if (!parent || parent->type != NBT_LIST) {
        i16 name_length;
        gzread(fd, &name_length, sizeof name_length);
        name_length = ntoh16(name_length);
        tag.name = str_alloc(name_length + 1, ctx->arena);
        gzread(fd, &tag.name.base, name_length);
        tag.name.length = name_length;
    }

    switch (type) {
    case NBT_BYTE:
        gzread(fd, &tag.data.simple.byte, sizeof tag.data.simple.byte);
        break;
    case NBT_SHORT: {
        i16 num;
        gzread(fd, &num, sizeof num);
        tag.data.simple.short_num = ntoh16(num);
        break;
    }
    case NBT_INT: {
        i32 num;
        gzread(fd, &num, sizeof num);
        tag.data.simple.integer = ntoh16(num);
        break;
    }
    case NBT_LONG: {
        i64 num;
        gzread(fd, &num, sizeof num);
        tag.data.simple.long_num = ntoh16(num);
        break;
    }
    case NBT_FLOAT:
        gzread(fd, &tag.data.simple.float_num, sizeof tag.data.simple.float_num);
        break;
    case NBT_DOUBLE:
        gzread(fd, &tag.data.simple.double_num, sizeof tag.data.simple.double_num);
        break;
    case NBT_STRING: {

        i16 name_length;
        gzread(fd, &name_length, sizeof name_length);
        name_length = ntoh16(name_length);
        string name = str_alloc(name_length + 1, ctx->arena);
        gzread(fd, &name.base, name_length);
        name.length = name_length;
        break;
    }
    case NBT_LIST: {
        i8 type_byte;
        gzread(fd, &type_byte, sizeof type_byte);
        enum NBTTagType elem_type = type_byte;
        tag.data.list.elem_type = elem_type;

        i32 num;
        gzread(fd, &num, sizeof num);
        tag.data.list.size = ntoh32(num);
        tag.data.list.total_tag_length = 1;

        NBTReadTag* new_tag = vector_reserve(&ctx->stack);
        *new_tag = (NBTReadTag){
            .size = tag.data.list.size,
            .type = NBT_LIST,
            .idx = ctx->nbt->tags.size,
        };

        break;
    }
    case NBT_BYTE_ARRAY:
    case NBT_INT_ARRAY:
    case NBT_LONG_ARRAY: {
        i32 num;
        gzread(fd, &num, sizeof num);
        tag.data.array_size = ntoh32(num);

        NBTReadTag* new_tag = vector_reserve(&ctx->stack);
        *new_tag = (NBTReadTag){
            .size = tag.data.array_size,
            .type = type,
            .idx = ctx->nbt->tags.size,
        };

        break;
    }
    case NBT_COMPOUND: {
        NBTReadTag* new_tag = vector_reserve(&ctx->stack);
        *new_tag = (NBTReadTag){
            .size = 0,
            .type = type,
            .idx = ctx->nbt->tags.size,
        };
        tag.data.compound.total_tag_length = 1;
        break;
    }
    case NBT_END:
        if (!parent || parent->type != NBT_COMPOUND) {
            log_errorf("Unexpected end of compound tag at position %zu", gztell(fd));
            return FALSE;
        }
        NBTTag* parent_tag = vector_ref(&ctx->nbt->tags, parent->idx);
        parent_tag->data.compound.size = parent->size;
        vector_pop(&ctx->stack, NULL);
        return TRUE;
    default:
        return TRUE;
    }

    vector_add(&ctx->nbt->tags, &tag);
    
    return TRUE;
}

bool nbt_parse(Arena* arena, i64 max_token_count, const string* path, NBT* out_nbt) {

    gzFile fd = gzopen(path->base, "rb");

    NBTReadContext ctx = {
        .arena = arena,
        .nbt = out_nbt,
    };
    vector_init_fixed(&ctx.stack, arena, 512, sizeof(NBTReadTag));

    *out_nbt = nbt_create(arena, max_token_count);

    do {

        i8 type_byte;
        gzread(fd, &type_byte, sizeof type_byte);
        enum NBTTagType type = type_byte;
        if (type >= _NBT_COUNT || type < NBT_END) {
            log_errorf("NBT: Lexical error: Invalid tag type %i", type);
            return FALSE;
        }

        NBTReadTag* parent = vector_ref(&ctx.stack, ctx.stack.size - 1);
        if (parent && type != NBT_END) {
            switch (parent->type) {
            case NBT_COMPOUND:
                parent->size++;
                break;
            case NBT_LIST:
            case NBT_BYTE_ARRAY:
            case NBT_INT_ARRAY:
            case NBT_LONG_ARRAY:
                parent->size--;
                break;
            default:
                break;
            }
        }

        if (!nbt_parse_tag(fd, &ctx, type))
            return FALSE;

        for(i64 i = (i64)ctx.stack.size - 1; i >= 0; i--) {
            parent = vector_ref(&ctx.stack, i);
            NBTTag* parent_tag = vector_ref(&out_nbt->tags, parent->idx);

            switch(parent->type) {
            case NBT_COMPOUND:
                parent_tag->data.compound.total_tag_length++;
                break;
            case NBT_LIST:
                parent_tag->data.list.total_tag_length++;
                EXPLICIT_FALLTHROUGH;
            case NBT_BYTE_ARRAY:
            case NBT_INT_ARRAY:
            case NBT_LONG_ARRAY:
                if(parent->size == 0) {
                    vector_remove(&ctx.stack, i, NULL);
                    i--;
                }
                break;
            default:
                log_errorf("Invalid parent tag of type %i", parent->type);
                return FALSE;
            }
            
        }
    } while (ctx.stack.size > 0);

    UNUSED(path);
    return TRUE;
}
