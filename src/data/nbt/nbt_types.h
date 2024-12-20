//
// Created by bmorino on 20/12/2024.
//

#ifndef NBT_TYPES_H
#define NBT_TYPES_H

#include "data/nbt.h"

typedef struct NBTTag {
    enum NBTTagType type;
    union NBTValue {
        union NBTSimpleValue simple;

        string str;
        struct nbt_list {
            enum NBTTagType elem_type;
            i32 total_tag_length;
            i32 size;
        } list;

        struct nbt_compound {
            i32 total_tag_length;
            i32 size;
        } compound;

        i32 array_size; /**< Used by NBT_BYTE_ARRAY, NBT_INT_ARRAY, NBT_LONG_ARRAY, NBT_COMPOUND */
    } data;
    string name;
} NBTTag;

#endif /* ! NBT_TYPES_H */
