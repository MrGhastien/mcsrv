#include "data/nbt.h"
#include "nbt_internal.h"
#include "utils/string.h"

#include <logger.h>
#include <stdio.h>
#include <string.h>

typedef struct SNBTContext {
    Vector stack;
    bool pretty_print;
    i32 spaces_per_indent;
} SNBTContext;

typedef struct SNBTMetadata {
    enum NBTTagType type;
    i32 size;
    const NBTTag* tag;
} SNBTMetadata;

static void add_meta(SNBTContext* ctx, enum NBTTagType type, i32 size, const NBTTag* tag_ref) {
    SNBTMetadata* new_meta = vector_reserve(&ctx->stack);
    *new_meta = (SNBTMetadata){
        .type = type,
        .size = size,
        .tag = tag_ref,
    };
}

static i32 get_tag_size(const NBTTag* tag) {
    switch (tag->type) {
    case NBT_COMPOUND:
        return tag->data.compound.size;
    case NBT_LIST:
        return tag->data.list.size;
    case NBT_STRING:
        return tag->data.str.length;
    case NBT_BYTE_ARRAY:
    case NBT_INT_ARRAY:
    case NBT_LONG_ARRAY:
        return tag->data.array_size;
    default:
        return 0;
    }
}

static void write_indent(FILE* fd, SNBTContext* ctx) {
    i32 max = ctx->stack.size * ctx->spaces_per_indent;
    for (i32 i = 0; i < max; ++i) {
        fputc(' ', fd);
    }
}

static void write_snbt_tag(const NBTTag* tag, FILE* fd, SNBTContext* ctx) {
    SNBTMetadata* parent = vector_ref(&ctx->stack, ctx->stack.size - 1);

    if (ctx->pretty_print) {
        if(parent) {
            if (parent->size < get_tag_size(parent->tag))
                fputc(',', fd);
            fputc('\n', fd);
        }
        write_indent(fd, ctx);
    }

    if (parent) {
        if (is_not_array(parent->type))
            fprintf(fd, "'%s': ", tag->name.base);
        parent->size--;
    }

    switch (tag->type) {
    case NBT_BYTE:
        fprintf(fd, "%hhxb", tag->data.simple.byte);
        break;
    case NBT_SHORT:
        fprintf(fd, "%his", tag->data.simple.short_num);
        break;
    case NBT_INT:
        fprintf(fd, "%i", tag->data.simple.integer);
        break;
    case NBT_LONG:
        fprintf(fd, "%llil", tag->data.simple.long_num);
        break;
    case NBT_FLOAT:
        fprintf(fd, "%f", tag->data.simple.float_num);
        break;
    case NBT_DOUBLE:
        fprintf(fd, "%lf", tag->data.simple.double_num);
        break;
    case NBT_STRING:
        fprintf(fd, "\"%s\"", tag->data.str.base);
        break;
    case NBT_LIST:
        fputc('[', fd);
        add_meta(ctx, tag->type, tag->data.list.size, tag);
        break;
    case NBT_COMPOUND:
        fputc('{', fd);
        add_meta(ctx, tag->type, tag->data.compound.size, tag);
        break;
    case NBT_BYTE_ARRAY:
        fputs("[B;", fd);
        add_meta(ctx, tag->type, tag->data.array_size, tag);
        break;
    case NBT_INT_ARRAY:
        fputs("[I;", fd);
        add_meta(ctx, tag->type, tag->data.array_size, tag);
        break;
    case NBT_LONG_ARRAY:
        fputs("[L;", fd);
        add_meta(ctx, tag->type, tag->data.array_size, tag);
        break;
    default:
        break;
    }
}

enum NBTStatus nbt_write_snbt(const NBT* nbt, const string* path) {
    FILE* fd = fopen(path->base, "w");
    if(fd == NULL) {
        log_errorf("NBT: IO Error: %s", strerror(errno));
        return NBTE_IO;
    }

    Arena scratch = arena_create(1 << 18);

    SNBTContext ctx = {
        .pretty_print = TRUE,
        .spaces_per_indent = 4,
    };
    vector_init_fixed(&ctx.stack, &scratch, 512, sizeof(SNBTMetadata));

    for (u32 i = 0; i < nbt->tags.size; i++) {
        const NBTTag* tag = nbt_ref(nbt, i);

        write_snbt_tag(tag, fd, &ctx);

        const SNBTMetadata* parent = vector_ref(&ctx.stack, ctx.stack.size - 1);
        while (parent && parent->size == 0) {
            vector_pop(&ctx.stack, NULL);
            fputc('\n', fd);
            switch (parent->type) {
            case NBT_COMPOUND:
                write_indent(fd, &ctx);
                fputc('}', fd);
                break;
            case NBT_LIST:
            case NBT_BYTE_ARRAY:
            case NBT_INT_ARRAY:
            case NBT_LONG_ARRAY:
                write_indent(fd, &ctx);
                fputc(']', fd);
                break;
            default:
                break;
            }
            parent = vector_ref(&ctx.stack, ctx.stack.size - 1);
        }
    }

    arena_destroy(&scratch);
    fclose(fd);
    return NBTE_OK;
}
enum NBTStatus nbt_to_string(const NBT* nbt, Arena* arena, string* out_str) {
    UNUSED(nbt);
    UNUSED(arena);
    UNUSED(out_str);
    return NBTE_OK;
}
