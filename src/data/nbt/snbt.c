#include "data/nbt.h"
#include "nbt_internal.h"
#include "utils/iomux.h"
#include "utils/string.h"

#include <logger.h>
#include <errno.h>
#include <inttypes.h>
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
    SNBTMetadata* new_meta = vect_reserve(&ctx->stack);
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

static void write_indent(IOMux fd, SNBTContext* ctx) {
    i32 max = ctx->stack.size * ctx->spaces_per_indent;
    for (i32 i = 0; i < max; ++i) {
        iomux_writec(fd, ' ');
    }
}

static void write_snbt_tag(const NBTTag* tag, IOMux fd, SNBTContext* ctx) {
    SNBTMetadata* parent = vect_ref(&ctx->stack, ctx->stack.size - 1);

    if (ctx->pretty_print) {
        if (parent) {
            if (parent->size < get_tag_size(parent->tag))
                iomux_writec(fd, ',');
            iomux_writec(fd, '\n');
        }
        write_indent(fd, ctx);
    }

    if (parent) {
        if (is_not_array(parent->type))
            iomux_writef(fd, "'%s': ", tag->name.base);
        parent->size--;
    }

    switch (tag->type) {
    case NBT_BYTE:
        iomux_writef(fd, "%hhxb", tag->data.simple.byte);
        break;
    case NBT_SHORT:
        iomux_writef(fd, "%his", tag->data.simple.short_num);
        break;
    case NBT_INT:
        iomux_writef(fd, "%i", tag->data.simple.integer);
        break;
    case NBT_LONG:
        iomux_writef(fd, "%" PRIi64 "l", tag->data.simple.long_num);
        break;
    case NBT_FLOAT:
        iomux_writef(fd, "%f", tag->data.simple.float_num);
        break;
    case NBT_DOUBLE:
        iomux_writef(fd, "%lf", tag->data.simple.double_num);
        break;
    case NBT_STRING:
        iomux_writef(fd, "\"%s\"", tag->data.str.base);
        break;
    case NBT_LIST:
        iomux_writec(fd, '[');
        add_meta(ctx, tag->type, tag->data.list.size, tag);
        break;
    case NBT_COMPOUND:
        iomux_writec(fd, '{');
        add_meta(ctx, tag->type, tag->data.compound.size, tag);
        break;
    case NBT_BYTE_ARRAY:
        iomux_writes(fd, "[B;");
        add_meta(ctx, tag->type, tag->data.array_size, tag);
        break;
    case NBT_INT_ARRAY:
        iomux_writes(fd, "[I;");
        add_meta(ctx, tag->type, tag->data.array_size, tag);
        break;
    case NBT_LONG_ARRAY:
        iomux_writes(fd, "[L;");
        add_meta(ctx, tag->type, tag->data.array_size, tag);
        break;
    default:
        break;
    }
}

enum NBTStatus nbt_write_snbt(const NBT* nbt, const string* path) {
    IOMux fd = iomux_open(path, "w");
    if (fd == -1) {
        log_errorf("NBT: IO Error: %s", strerror(errno));
        return NBTE_IO;
    }

    Arena scratch = arena_create(1 << 18);

    SNBTContext ctx = {
        .pretty_print = TRUE,
        .spaces_per_indent = 4,
    };
    vect_init(&ctx.stack, &scratch, 512, sizeof(SNBTMetadata));

    for (u32 i = 0; i < nbt->tags.size; i++) {
        const NBTTag* tag = nbt_ref(nbt, i);

        write_snbt_tag(tag, fd, &ctx);

        const SNBTMetadata* parent = vect_ref(&ctx.stack, ctx.stack.size - 1);
        while (parent && parent->size == 0) {
            vect_pop(&ctx.stack, NULL);
            iomux_writec(fd, '\n');
            switch (parent->type) {
            case NBT_COMPOUND:
                write_indent(fd, &ctx);
                iomux_writec(fd, '}');
                break;
            case NBT_LIST:
            case NBT_BYTE_ARRAY:
            case NBT_INT_ARRAY:
            case NBT_LONG_ARRAY:
                write_indent(fd, &ctx);
                iomux_writec(fd, ']');
                break;
            default:
                break;
            }
            parent = vect_ref(&ctx.stack, ctx.stack.size - 1);
        }
    }

    arena_destroy(&scratch);
    iomux_close(fd);
    return NBTE_OK;
}
enum NBTStatus nbt_to_string(const NBT* nbt, Arena* arena, string* out_str) {
    UNUSED(nbt);
    UNUSED(arena);
    UNUSED(out_str);
    return NBTE_OK;
}
