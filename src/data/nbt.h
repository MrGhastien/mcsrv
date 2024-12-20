//
// Created by bmorino on 20/12/2024.
//

#ifndef NBT_H
#define NBT_H

#include "containers/vector.h"

#include <utils/string.h>

enum NBTTagType {
    NBT_END = 0,
    NBT_BYTE,
    NBT_SHORT,
    NBT_INT,
    NBT_LONG,
    NBT_FLOAT,
    NBT_DOUBLE,
    NBT_BYTE_ARRAY,
    NBT_STRING,
    NBT_LIST,
    NBT_COMPOUND,
    NBT_INT_ARRAY,
    NBT_LONG_ARRAY,
};

union NBTSimpleValue {
    i8 byte;
    i16 short_num;
    i32 integer;
    i64 long_num;
    f32 float_num;
    f64 double_num;
};

typedef struct NBT {
    Vector tags;
    Arena* arena;
    Vector stack;
} NBT;

/* === Building part === */

/**
* Creates a new NBT tree.
*
* @param[in] arena The arena used to allocate memory for the NBT tree.
* @param[in] max_token_count Maximum amount of tags (nodes) in this NBT tree.
* @return The newly initialized NBT tree.
*/
NBT nbt_create(Arena* arena, u64 max_token_count);

/**
* Adds a simple value tag to the current NBT list.
*
* If the current tag is not a list or an array, this function does nothing.
*
* @param[in] nbt The NBT tree containing the list or array to push into.
* @param[in] type The type of tag to push.
* @param[in] value An union containing the value to push. Ignored if @ref type is not an integer or floating point number.
*/
void nbt_push_simple(NBT* nbt, enum NBTTagType type, union NBTSimpleValue value);
/**
* Adds a string tag to the current NBT list.
*
* If the current tag is not a list this function does nothing.
*
* @param[in] nbt The NBT tree containing the list to push into.
* @param[in] str The string to push.
*/
void nbt_push_str(NBT* nbt, const string* str);
/**
* Adds a tag to the current NBT list.
*
* The newly pushed tag is initialized to zero. It can be set using the `nbt_set_*` family of functions.
* If the current tag is not a list this function does nothing.
*
* @param[in] nbt The NBT tree containing the list to push into.
* @param[in] type The type of tag to push.
*/
void nbt_push(NBT* nbt, enum NBTTagType type);

/**
* Adds a simple value tag to the current compound NBT.
*
* If the current tag is not a compound this function does nothing.
*
* @param[in] nbt The NBT tree containing the compound to put into.
* @param[in] name The name of the tag to put into the current compound.
* @param[in] type The type of the tag.
* @param[in] value An union containing the value to put. Ignored if @ref type is not an integer or floating point number type.
*/
void nbt_put_simple(NBT* nbt, const string* name, enum NBTTagType type, union NBTSimpleValue value);
/**
* Adds a string tag to the current compound NBT.
*
* If the current tag is not a compound this function does nothing.
*
* @param[in] nbt The NBT tree containing the compound to put into.
* @param[in] name The name of the tag to put into the current compound.
* @param[in] str The string value of the tag.
*/
void nbt_put_str(NBT* nbt, const string* name, const string* str);
/**
* Adds a tag to the current compound NBT.
*
* If the current tag is not a compound this function does nothing.
*
* @param[in] nbt The NBT tree containing the compound to put into.
* @param[in] name The name of the tag to put into the current compound.
* @param[in] type The type of the tag.
*/
void nbt_put(NBT* nbt, const string* name, enum NBTTagType type);

/**
* Sets the value of the current byte tag.
*
* @param[in] nbt The NBT tree containing the tag to set.
* @param[in] value The value to set the tag to.
*/
void nbt_set_byte(NBT* nbt, i32 value);

/**
* Convenience function to set the current byte tag to a boolean value.
*
* The byte tag is set to `1` if @p value is @ref TRUE, otherwise it is set to `0`.
*
* @param[in] nbt The NBT tree containing the tag to set.
* @param[in] value The value to set the tag to.
*/
void nbt_set_bool(NBT* nbt, bool value);

/**
* Sets the value of the current short integer tag.
*
* @param[in] nbt The NBT tree containing the tag to set.
* @param[in] value The value to set the tag to.
*/
void nbt_set_short(NBT* nbt, i32 value);

/**
* Sets the value of the current integer tag.
*
* @param[in] nbt The NBT tree containing the tag to set.
* @param[in] value The value to set the tag to.
*/
void nbt_set_int(NBT* nbt, i32 value);

/**
* Sets the value of the current long integer tag.
*
* @param[in] nbt The NBT tree containing the tag to set.
* @param[in] value The value to set the tag to.
*/
void nbt_set_long(NBT* nbt, i64 value);

/**
* Sets the value of the current floating-point number tag.
*
* @param[in] nbt The NBT tree containing the tag to set.
* @param[in] value The value to set the tag to.
*/
void nbt_set_float(NBT* nbt, f32 value);

/**
* Sets the value of the current double-precision floating-point number tag.
*
* @param[in] nbt The NBT tree containing the tag to set.
* @param[in] value The value to set the tag to.
*/
void nbt_set_double(NBT* nbt, f64 value);

/**
* Writes a NBT tree to a file.
*
* @param[in] nbt The NBT tree to save.
* @param[in] path The path of the output file.
*/
void nbt_write(const NBT* nbt, const string* path);

/* === Parsing part === */

NBT nbt_parse(Arena* arena, i64 max_token_count, const string* path);

void nbt_move_to_name(NBT* nbt, const string* name);
void nbt_move_to_index(NBT* nbt, i32 index);
void nbt_move_to_parent(NBT* nbt);
void nbt_move_to_next_sibling(NBT* nbt);
void nbt_move_to_prev_sibling(NBT* nbt);

i8 nbt_get_byte(NBT* nbt);
inline bool nbt_get_bool(NBT* nbt) {
    return nbt_get_byte(nbt);
}
i16 nbt_get_short(NBT* nbt);
i32 nbt_get_int(NBT* nbt);
i64 nbt_get_long(NBT* nbt);
f32 nbt_get_float(NBT* nbt);
f64 nbt_get_double(NBT* nbt);

string* nbt_get_name(NBT* nbt);
string* nbt_get_string(NBT* nbt);

#endif /* ! NBT_H */
