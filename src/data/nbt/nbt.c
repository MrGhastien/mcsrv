//
// Created by bmorino on 20/12/2024.
//

#include "data/nbt.h"
#include "definitions.h"
#include "nbt_types.h"

#include "utils/string.h"

#include <logger.h>
#include <stdio.h>
#include <stdlib.h>

static NBTTag* get_current_tag(const NBT* nbt) {
    i64 idx;
    if (!vector_peek(&nbt->stack, &idx))
        return NULL;

    return vector_ref(&nbt->tags, idx);
}

static void increment_parent_total_lengths(NBT* nbt) {
    for (u32 i = 0; i < nbt->stack.size; i++) {
        u64 idx;
        vector_get(&nbt->stack, i, &idx);
        NBTTag* tag = vector_ref(&nbt->tags, idx);

        switch (tag->type) {
        case NBT_COMPOUND:
            tag->data.compound.total_tag_length++;
            break;
        case NBT_LIST:
            tag->data.list.total_tag_length++;
            break;
        default:
            log_fatalf("NBTag of type %i cannot be in the NBT stack !", tag->type);
            abort();
            break;
        }
    }
}

static i32 get_total_length(const NBTTag* tag) {
    switch (tag->type) {
    case NBT_COMPOUND:
        return tag->data.compound.total_tag_length;
    case NBT_LIST:
        return tag->data.list.total_tag_length;
    case NBT_BYTE_ARRAY:
    case NBT_INT_ARRAY:
    case NBT_LONG_ARRAY:
        return 1 + tag->data.array_size;
    default:
        return 1;
    }
}

NBT nbt_create(Arena* arena, u64 max_token_count) {
    NBT nbt;
    nbt.arena = arena;
    vector_init_fixed(&nbt.tags, arena, max_token_count, sizeof(NBTTag));
    vector_init_fixed(&nbt.stack, arena, 512, sizeof(i64));

    NBTTag* root = vector_reserve(&nbt.tags);
    *root = (NBTTag){
        .type = NBT_COMPOUND,
        .data.compound.total_tag_length = 1,
        .data.compound.size = 0,
    };

    vector_add_imm(&nbt.stack, 0LL, i64);

    return nbt;
}

void nbt_push_simple(NBT* nbt, enum NBTTagType type, union NBTSimpleValue value) {
    NBTTag* tag = get_current_tag(nbt);
    switch (tag->type) {
    case NBT_LIST:
        if (tag->data.list.elem_type == NBT_END)
            tag->data.list.elem_type = type;
        else if (tag->data.list.elem_type != type)
            return;
        break;
    case NBT_BYTE_ARRAY:
        if (type != NBT_BYTE)
            return;
        break;
    case NBT_INT_ARRAY:
        if (type != NBT_INT)
            return;
        break;
    case NBT_LONG_ARRAY:
        if (type != NBT_LONG)
            return;
        break;
    default:
        return;
    }

    NBTTag new_tag = {
        .type = tag->data.list.elem_type,
        .data.simple = value,
    };
    vector_add(&nbt->tags, &new_tag);
    tag->data.list.size++;
    increment_parent_total_lengths(nbt);
}
void nbt_push_str(NBT* nbt, const string* str) {
    NBTTag* tag = get_current_tag(nbt);
    if (tag->type != NBT_LIST)
        return;

    if (tag->data.list.elem_type == NBT_END)
        tag->data.list.elem_type = NBT_STRING;
    if (tag->data.list.elem_type != NBT_STRING)
        return;

    NBTTag new_tag = {
        .type = tag->data.list.elem_type,
        .data.str = str_create_copy(str, nbt->arena),
    };
    vector_add(&nbt->tags, &new_tag);
    tag->data.list.size++;
}
void nbt_push(NBT* nbt, enum NBTTagType type) {
    NBTTag* tag = get_current_tag(nbt);
    if (tag->type != NBT_LIST)
        return;

    if (tag->data.list.elem_type == NBT_END)
        tag->data.list.elem_type = type;
    else if (tag->data.list.elem_type != type)
        return;

    NBTTag new_tag = {
        .type = tag->data.list.elem_type,
    };
    vector_add(&nbt->tags, &new_tag);
    tag->data.list.size++;
}

void nbt_put_simple(NBT* nbt,
                    const string* name,
                    enum NBTTagType type,
                    union NBTSimpleValue value) {
    NBTTag* tag = get_current_tag(nbt);
    if (tag->type != NBT_COMPOUND)
        return;

    NBTTag new_tag = {
        .type = type,
        .data.simple = value,
        .name = str_create_copy(name, nbt->arena),
    };
    vector_add(&nbt->tags, &new_tag);
    tag->data.compound.size++;
}
void nbt_put_str(NBT* nbt, const string* name, const string* str) {
    NBTTag* tag = get_current_tag(nbt);
    if (tag->type != NBT_COMPOUND)
        return;

    NBTTag new_tag = {
        .type = tag->data.list.elem_type,
        .data.str = str_create_copy(str, nbt->arena),
        .name = str_create_copy(name, nbt->arena),
    };
    vector_add(&nbt->tags, &new_tag);
    tag->data.compound.size++;
}

void nbt_put(NBT* nbt, const string* name, enum NBTTagType type) {
    NBTTag* tag = get_current_tag(nbt);
    if (tag->type != NBT_COMPOUND)
        return;

    NBTTag new_tag = {
        .type = type,
        .name = str_create_copy(name, nbt->arena),
    };
    if(type == NBT_COMPOUND)
        new_tag.data.compound.total_tag_length = 1;
    else if (type == NBT_LIST)
        new_tag.data.list.total_tag_length = 1;
    vector_add(&nbt->tags, &new_tag);
    tag->data.compound.size++;
}

void nbt_set_byte(NBT* nbt, i32 value) {
    NBTTag* tag = get_current_tag(nbt);
    if (tag->type != NBT_BYTE)
        return;

    tag->data.simple.byte = (i8) (value & 0xff);
}
void nbt_set_bool(NBT* nbt, bool value) {
    NBTTag* tag = get_current_tag(nbt);
    if (tag->type != NBT_BYTE)
        return;

    tag->data.simple.byte = value == 0 ? 0 : 1;
}
void nbt_set_short(NBT* nbt, i32 value) {
    NBTTag* tag = get_current_tag(nbt);
    if (tag->type != NBT_SHORT)
        return;

    tag->data.simple.short_num = (i16) (value & 0xffff);
}
void nbt_set_int(NBT* nbt, i32 value) {
    NBTTag* tag = get_current_tag(nbt);
    if (tag->type != NBT_INT)
        return;

    tag->data.simple.integer = value;
}
void nbt_set_long(NBT* nbt, i64 value) {
    NBTTag* tag = get_current_tag(nbt);
    if (tag->type != NBT_LONG)
        return;

    tag->data.simple.long_num = value;
}
void nbt_set_float(NBT* nbt, f32 value) {
    NBTTag* tag = get_current_tag(nbt);
    if (tag->type != NBT_FLOAT)
        return;

    tag->data.simple.float_num = value;
}
void nbt_set_double(NBT* nbt, f64 value) {
    NBTTag* tag = get_current_tag(nbt);
    if (tag->type != NBT_DOUBLE)
        return;

    tag->data.simple.double_num = value;
}

void nbt_move_to_name(NBT* nbt, const string* name) {
    NBTTag* tag = get_current_tag(nbt);

    if (tag->type != NBT_COMPOUND)
        return;

    i64 idx;
    vector_peek(&nbt->stack, &idx);
    idx++;
    for (i32 i = 0; i < tag->data.array_size; i++) {
        NBTTag* child_tag = vector_ref(&nbt->tags, idx);
        if (str_compare(&child_tag->name, name) == 0) {
            vector_add(&nbt->stack, &idx);
            return;
        }

        idx += get_total_length(child_tag);
    }

    log_errorf("Could not find NBTag with name %s.", name->base);
}
void nbt_move_to_index(NBT* nbt, i32 index) {
    NBTTag* tag = get_current_tag(nbt);

    if (tag->type != NBT_COMPOUND)
        return;

    if (index >= tag->data.list.size) {
        log_errorf("NBT: Index %i is out of the list's bounds.", index);
        return;
    }

    i64 idx;
    vector_peek(&nbt->stack, &idx);
    for (i32 i = 0; i < index; i++) {
        NBTTag* child_tag = vector_ref(&nbt->tags, idx);
        idx += get_total_length(child_tag);
    }
    vector_add(&nbt->stack, &idx);
}
void nbt_move_to_parent(NBT* nbt) {
    vector_pop(&nbt->stack, NULL);
}
void nbt_move_to_next_sibling(NBT* nbt) {
    NBTTag* tag = get_current_tag(nbt);
    i64 prev_index;
    vector_pop(&nbt->stack, &prev_index);
    prev_index += get_total_length(tag);
    vector_add(&nbt->stack, &prev_index);
}
void nbt_move_to_prev_sibling(NBT* nbt) {
    UNUSED(nbt);
    abort();
}
i8 nbt_get_byte(NBT* nbt) {
    NBTTag* tag = get_current_tag(nbt);
    if (tag->type != NBT_BYTE) {
        log_fatalf("Cannot get byte value of tag of type %i", tag->type);
        abort();
    }

    return tag->data.simple.byte;
}
i16 nbt_get_short(NBT* nbt) {
    NBTTag* tag = get_current_tag(nbt);
    if (tag->type != NBT_SHORT) {
        log_fatalf("Cannot get short integer value of tag of type %i", tag->type);
        abort();
    }

    return tag->data.simple.byte;
}
i32 nbt_get_int(NBT* nbt) {

    NBTTag* tag = get_current_tag(nbt);
    if (tag->type != NBT_INT) {
        log_fatalf("Cannot get integer value of tag of type %i", tag->type);
        abort();
    }

    return tag->data.simple.byte;
}
i64 nbt_get_long(NBT* nbt) {
    NBTTag* tag = get_current_tag(nbt);
    if (tag->type != NBT_LONG) {
        log_fatalf("Cannot get integer value of tag of type %i", tag->type);
        abort();
    }

    return tag->data.simple.byte;
}
f32 nbt_get_float(NBT* nbt) {
    NBTTag* tag = get_current_tag(nbt);
    if (tag->type != NBT_FLOAT) {
        log_fatalf("Cannot get float value of tag of type %i", tag->type);
        abort();
    }

    return tag->data.simple.float_num;
}
f64 nbt_get_double(NBT* nbt) {
    NBTTag* tag = get_current_tag(nbt);
    if (tag->type != NBT_DOUBLE) {
        log_fatalf("Cannot get double-float value of tag of type %i", tag->type);
        abort();
    }

    return tag->data.simple.double_num;
}
string* nbt_get_name(NBT* nbt) {
    NBTTag* tag = get_current_tag(nbt);
    return &tag->name;
}
string* nbt_get_string(NBT* nbt) {
    NBTTag* tag = get_current_tag(nbt);
    if (tag->type != NBT_STRING) {
        log_fatalf("Cannot get string value of tag of type %i", tag->type);
        abort();
    }

    return &tag->data.str;
}
