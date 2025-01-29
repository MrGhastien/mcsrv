//
// Created by bmorino on 20/12/2024.
//

#include "data/nbt.h"
#include "definitions.h"
#include "nbt_internal.h"

#include "utils/string.h"

#include <logger.h>
#include <stdio.h>
#include <stdlib.h>

static NBTTag* get_current_tag(NBT* nbt) {
    i64 idx;
    if (!vect_peek(&nbt->stack, &idx))
        return NULL;

    return vect_ref(&nbt->tags, idx);
}

static void increment_parent_total_lengths(NBT* nbt) {
    for (u32 i = 0; i < nbt->stack.size; i++) {
        u64 idx;
        vect_get(&nbt->stack, i, &idx);
        NBTTag* tag = vect_ref(&nbt->tags, idx);

        switch (tag->type) {
        case NBT_COMPOUND:
        case NBT_LIST:
            tag->data.composite.total_tag_length++;
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
    case NBT_LIST:
        return tag->data.composite.total_tag_length;
    case NBT_BYTE_ARRAY:
    case NBT_INT_ARRAY:
    case NBT_LONG_ARRAY:
        return 1 + tag->data.array_size;
    default:
        return 1;
    }
}

static void append_tag(NBT* nbt, NBTTag* new_tag) {
    NBTTag* parent = get_current_tag(nbt);
    new_tag->local_idx = nbt_get_size(nbt);
    vect_peek(&nbt->stack, &new_tag->parent_idx);
    new_tag->prev_sibling_idx = parent->data.composite.last_child;
    vect_add(&nbt->tags, new_tag);
    parent->data.composite.size++;
    increment_parent_total_lengths(nbt);
}

bool is_not_array(const enum NBTTagType type) {
    return !(type == NBT_LIST || type == NBT_BYTE_ARRAY || type == NBT_INT_ARRAY ||
             type == NBT_LONG_ARRAY);
}

// static bool is_composite(const NBTTag* tag) {
//     return tag->type == NBT_LIST || tag->type == NBT_BYTE_ARRAY || tag->type == NBT_INT_ARRAY ||
//            tag->type == NBT_LONG_ARRAY || tag->type == NBT_COMPOUND;
// }

NBT nbt_create(Arena* arena, u64 max_token_count) {
    NBT nbt;
    nbt_init_empty(arena, max_token_count, &nbt);

    NBTTag* root = vect_reserve(&nbt.tags);
    *root = (NBTTag){
        .type = NBT_COMPOUND,
        .data.composite = {
            .total_tag_length = 1,
            .last_child = -1,
        },
    };

    vect_add_imm(&nbt.stack, 0LL, i64);

    return nbt;
}

void nbt_init_empty(Arena* arena, u64 max_token_count, NBT* out_nbt) {
    out_nbt->arena = arena;
    vect_init_dynamic(&out_nbt->tags, arena, max_token_count, sizeof(NBTTag));
    vect_init(&out_nbt->stack, arena, 512, sizeof(i64));
}

void nbt_add_tag(NBT* nbt, NBTTag* tag) {
    vect_add(&nbt->tags, tag);
}

const NBTTag* nbt_ref(const NBT* nbt, i64 index) {
    return vect_ref(&nbt->tags, index);
}

NBTTag* nbt_mut_ref(NBT* nbt, i64 index) {
    return vect_ref(&nbt->tags, index);
}

enum NBTStatus nbt_push_simple(NBT* nbt, enum NBTTagType type, union NBTSimpleValue value) {
    NBTTag* tag = get_current_tag(nbt);
    switch (tag->type) {
    case NBT_LIST:
        if (tag->data.list.elem_type == NBT_END)
            tag->data.list.elem_type = type;
        else if (tag->data.list.elem_type != type)
            return NBTE_INCOMPATIBLE_TYPE;
        break;
    case NBT_BYTE_ARRAY:
        if (type != NBT_BYTE)
            return NBTE_INCOMPATIBLE_TYPE;
        break;
    case NBT_INT_ARRAY:
        if (type != NBT_INT)
            return NBTE_INCOMPATIBLE_TYPE;
        break;
    case NBT_LONG_ARRAY:
        if (type != NBT_LONG)
            return NBTE_INCOMPATIBLE_TYPE;
        break;
    default:
        return NBTE_INVALID_TYPE;
    }

    NBTTag new_tag = {
        .type = tag->data.list.elem_type,
        .data.simple = value,
    };
    append_tag(nbt, &new_tag);
    return TRUE;
}
enum NBTStatus nbt_push_str(NBT* nbt, const string* str) {
    NBTTag* tag = get_current_tag(nbt);
    if (tag->type != NBT_LIST)
        return NBTE_INVALID_PARENT;

    if (tag->data.list.elem_type == NBT_END)
        tag->data.list.elem_type = NBT_STRING;
    if (tag->data.list.elem_type != NBT_STRING)
        return NBTE_INCOMPATIBLE_TYPE;

    NBTTag new_tag = {
        .type = tag->data.list.elem_type,
        .data.str = str_create_copy(str, nbt->arena),
    };
    append_tag(nbt, &new_tag);
    return NBTE_OK;
}
enum NBTStatus nbt_push(NBT* nbt, enum NBTTagType type) {
    NBTTag* tag = get_current_tag(nbt);
    if (tag->type != NBT_LIST)
        return NBTE_INVALID_PARENT;

    if (tag->data.list.elem_type == NBT_END)
        tag->data.list.elem_type = type;
    else if (tag->data.list.elem_type != type)
        return NBTE_INCOMPATIBLE_TYPE;

    NBTTag new_tag = {
        .type = tag->data.list.elem_type,
    };
    append_tag(nbt, &new_tag);
    return NBTE_OK;
}

enum NBTStatus
nbt_put_simple(NBT* nbt, const string* name, enum NBTTagType type, union NBTSimpleValue value) {
    NBTTag* tag = get_current_tag(nbt);
    if (tag->type != NBT_COMPOUND)
        return NBTE_INVALID_PARENT;

    NBTTag new_tag = {
        .type = type,
        .data.simple = value,
        .name = str_create_copy(name, nbt->arena),
    };
    append_tag(nbt, &new_tag);
    return NBTE_OK;
}
enum NBTStatus nbt_put_str(NBT* nbt, const string* name, const string* str) {
    NBTTag* tag = get_current_tag(nbt);
    if (tag->type != NBT_COMPOUND)
        return NBTE_INVALID_PARENT;

    NBTTag new_tag = {
        .type = tag->data.list.elem_type,
        .data.str = str_create_copy(str, nbt->arena),
        .name = str_create_copy(name, nbt->arena),
    };
    append_tag(nbt, &new_tag);
    return NBTE_OK;
}

enum NBTStatus nbt_put(NBT* nbt, const string* name, enum NBTTagType type) {
    NBTTag* tag = get_current_tag(nbt);
    if (tag->type != NBT_COMPOUND)
        return NBTE_INVALID_PARENT;

    NBTTag new_tag = {
        .type = type,
        .name = str_create_copy(name, nbt->arena),
    };
    if (type == NBT_COMPOUND)
        new_tag.data.composite.total_tag_length = 1;
    else if (type == NBT_LIST)
        new_tag.data.composite.total_tag_length = 1;
    append_tag(nbt, &new_tag);
    return NBTE_OK;
}

enum NBTStatus nbt_set_byte(NBT* nbt, i32 value) {
    NBTTag* tag = get_current_tag(nbt);
    if (tag->type != NBT_BYTE)
        return NBTE_INCOMPATIBLE_TYPE;

    tag->data.simple.byte = (i8) (value & 0xff);
    return NBTE_OK;
}
enum NBTStatus nbt_set_bool(NBT* nbt, bool value) {
    NBTTag* tag = get_current_tag(nbt);
    if (tag->type != NBT_BYTE)
        return NBTE_INCOMPATIBLE_TYPE;

    tag->data.simple.byte = value == 0 ? 0 : 1;
    return NBTE_OK;
}
enum NBTStatus nbt_set_short(NBT* nbt, i32 value) {
    NBTTag* tag = get_current_tag(nbt);
    if (tag->type != NBT_SHORT)
        return NBTE_INCOMPATIBLE_TYPE;

    tag->data.simple.short_num = (i16) (value & 0xffff);
    return NBTE_OK;
}
enum NBTStatus nbt_set_int(NBT* nbt, i32 value) {
    NBTTag* tag = get_current_tag(nbt);
    if (tag->type != NBT_INT)
        return NBTE_INCOMPATIBLE_TYPE;

    tag->data.simple.integer = value;
    return NBTE_OK;
}
enum NBTStatus nbt_set_long(NBT* nbt, i64 value) {
    NBTTag* tag = get_current_tag(nbt);
    if (tag->type != NBT_LONG)
        return NBTE_INCOMPATIBLE_TYPE;

    tag->data.simple.long_num = value;
    return NBTE_OK;
}
enum NBTStatus nbt_set_float(NBT* nbt, f32 value) {
    NBTTag* tag = get_current_tag(nbt);
    if (tag->type != NBT_FLOAT)
        return NBTE_INCOMPATIBLE_TYPE;

    tag->data.simple.float_num = value;
    return NBTE_OK;
}
enum NBTStatus nbt_set_double(NBT* nbt, f64 value) {
    NBTTag* tag = get_current_tag(nbt);
    if (tag->type != NBT_DOUBLE)
        return NBTE_INCOMPATIBLE_TYPE;

    tag->data.simple.double_num = value;
    return NBTE_OK;
}

enum NBTStatus nbt_move_to_name(NBT* nbt, const string* name) {
    NBTTag* tag = get_current_tag(nbt);

    if (tag->type != NBT_COMPOUND)
        return NBTE_INVALID_PARENT;

    i64 idx;
    vect_peek(&nbt->stack, &idx);
    idx++;
    for (i32 i = 0; i < tag->data.array_size; i++) {
        NBTTag* child_tag = vect_ref(&nbt->tags, idx);
        if (str_compare(&child_tag->name, name) == 0) {
            vect_add(&nbt->stack, &idx);
            return NBTE_OK;
        }

        idx += get_total_length(child_tag);
    }

    log_errorf("Could not find NBTag with name %s.", name->base);
    return NBTE_NOT_FOUND;
}
enum NBTStatus nbt_move_to_cstr(NBT* nbt, const char* name) {
    string str = str_view(name);
    return nbt_move_to_name(nbt, &str);
}
enum NBTStatus nbt_move_to_index(NBT* nbt, i32 index) {
    NBTTag* tag = get_current_tag(nbt);

    if (tag->type != NBT_LIST)
        return NBTE_INVALID_PARENT;

    if (index >= tag->data.composite.size || index < 0) {
        log_errorf("NBT: Index %i is out of the list's bounds.", index);
        return NBTE_NOT_FOUND;
    }

    i64 idx;
    vect_peek(&nbt->stack, &idx);
    for (i32 i = 0; i < index; i++) {
        NBTTag* child_tag = vect_ref(&nbt->tags, idx);
        idx += get_total_length(child_tag);
    }
    vect_add(&nbt->stack, &idx);
    return NBTE_OK;
}
enum NBTStatus nbt_move_to_parent(NBT* nbt) {
    return vect_pop(&nbt->stack, NULL) ? NBTE_OK : NBTE_NOT_FOUND;
}
enum NBTStatus nbt_move_to_next_sibling(NBT* nbt) {
    NBTTag* tag = get_current_tag(nbt);
    i64 global_index;
    if (!vect_pop(&nbt->stack, &global_index))
        return NBTE_NOT_FOUND;
    enum NBTStatus status = NBTE_OK;
    if (tag->local_idx + 1 > (i64)nbt_get_size(nbt))
        status = NBTE_NOT_FOUND;
    else
        global_index += get_total_length(tag);
    vect_add(&nbt->stack, &global_index);
    return status;
}
enum NBTStatus nbt_move_to_prev_sibling(NBT* nbt) {
    NBTTag* tag = get_current_tag(nbt);
    i64 global_index;
    if (!vect_pop(&nbt->stack, &global_index))
        return NBTE_NOT_FOUND;
    enum NBTStatus status = NBTE_OK;
    if (tag->local_idx == 0)
        status = NBTE_NOT_FOUND;
    else
        global_index = tag->prev_sibling_idx;
    vect_add(&nbt->stack, &global_index);
    return status;
}

enum NBTTagType nbt_get_type(NBT* nbt) {
    NBTTag* tag = get_current_tag(nbt);
    return tag->type;
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

u64 nbt_get_size(NBT* nbt) {
    NBTTag* tag = get_current_tag(nbt);
    switch (tag->type) {
    case NBT_LIST:
    case NBT_COMPOUND:
        return tag->data.composite.size;
    case NBT_BYTE_ARRAY:
    case NBT_INT_ARRAY:
    case NBT_LONG_ARRAY:
        return tag->data.array_size;
    case NBT_STRING:
        return tag->data.str.length;
    default:
        log_errorf("Cannot get size of tag of type %i", tag->type);
        return -1;
    }
}
string* nbt_get_name(NBT* nbt) {
    NBTTag* tag = get_current_tag(nbt);
    NBTTag* parent = vect_ref(&nbt->stack, nbt->stack.size - 1);
    if (!parent || is_not_array(parent->type))
        return NULL;
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
