/**
 * @file
 * Internal functions and types related to NBT trees.
 *
 * These functions are meant to be used only within the NBT module.
 */

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

bool is_not_array(const enum NBTTagType type);

/**
* Initializes a NBT tree without any root component.
*
* @param[in] arena The arena used to allocate memory for the NBT tree.
* @param[in] max_token_count Maximum amount of tags (nodes) in this NBT tree.
* @param[out] out_nbt A pointer to the NBT tree to initialize. Cannot be `NULL`.
*/
void nbt_init_empty(Arena* arena, u64 max_token_count, NBT* out_nbt);

/**
* Adds a new tag to the underlying array of the tree.
*
* @param[in] nbt the NBT tree to add a tag into.
* @param[in] tag The tag to add.
*/
void nbt_add_tag(NBT* nbt, NBTTag* tag);

/**
* Retrieves a reference of a tag inside the NBT tree.
*
* @param[in] nbt the NBT tree to get the reference from.
* @param[in] index The index of the tag to get in the tree's underlying array.
* @return A pointer to the tag at the given index in the tree's array, or `NULL` if the index is invalid or there is no tag there.
*/
const NBTTag* nbt_ref(const NBT* nbt, i64 index);
NBTTag* nbt_mut_ref(NBT* nbt, i64 index);

#endif /* ! NBT_TYPES_H */
