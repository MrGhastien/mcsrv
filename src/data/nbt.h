/**
 * @file
 *
 * Interface for creating, saving and loading Named Binary Tag (NBT) trees.
 *
 * ### Interface description
*
 * In this interface, NBT trees always have one tag selected. Most operations on NBT tags operate
 * on the selected tag, and there are specific functions to change the selection.
 *
 * This implementation uses a single resizable array to store tags, and uses a stack to keep
 * track of the selection and parent-child relationships between tags.
 *
 * ### Walkthrough
 * #### Initialization
 * Trees can be created by calling the @ref nbt_create function.
 * By default, a newly created NBT tree will contain an empty compound tag.
 *
 * #### Tag creation
 * You can add child tags to compound tags by calling @ref nbt_put, @ref nbt_put_simple or
 * @ref nbt_put_str, and to list tags using the functions @ref nbt_push, @ref nbt_push_simple and
 * @ref nbt_push_str.
 *
 * *The functions listed above operate on the current tag of the NBT tree.*
 *
 * #### Navigation / Modifying the selection
 * You can navigate between children/parents by calling @ref nbt_move_to_name (for compound tags),
 * @ref nbt_move_to_index (for list and array tags) and @ref nbt_move_to_parent.
 *
 * You can also navigate between siblings : @ref nbt_move_to_next_sibling, @ref nbt_move_to_prev_sibling.
 *
 * #### Querying tag values
 *
 * You can get the value of any tag (except compound, list and array tags) by using the
 * `nbt_get_*` family of functions:
 * - @ref nbt_get_byte
 * - @ref nbt_get_short
 * - @ref nbt_get_int
 * - @ref nbt_get_long
 * - @ref nbt_get_float
 * - @ref nbt_get_double
 * - @ref nbt_get_string
 *
 * The @ref nbt_get_bool fonction is a convenience shortcut to use `NBT_BYTE` tags as boolean tags,
 * as boolean tags do not actually exist.
 *
 * The name of any tags (*including* compound, list and array tags) can also be retrieved using
 * @ref nbt_get_name.
 *
 * *These functions operate on the selected tag of the NBT tree.*
 *
 * #### Modifying tag values
 * The value of any tag (except compound, list and array tags) can be changed.
 *
 * For number types, you can use the `nbt_set_*` family of functions:
 * - @ref nbt_set_byte
 * - @ref nbt_set_short
 * - @ref nbt_set_int
 * - @ref nbt_set_long
 * - @ref nbt_set_float
 * - @ref nbt_set_double
 *
 * To modify string values and tag names, simply modify the underlying string object you retrieved
 * by calling @ref nbt_get_string or @ref nbt_get_name using @ref string.h "string functions".
 *
 * *These functions operate on the selected tag of the NBT tree.*
 *
 * ### Example
 *
 * This example will attempt to create the following NBT tree :
 * @code
 * COMPOUND {
 *     "data": COMPOUND {
 *         "numbers": LIST<INT> = [
 *             [0] = 3
 *             [1] = 17
 *         ]
 *         "x": STRING = "hello!"
 *     }
 * }
 * @endcode
 * First, we create a new NBT tree :
 * @code{c}
 * Arena arena = arena_create(1 << 15);
 * NBT nbt = nbt_create(&arena, 1024);
 * @endcode
 *
 * Next, we add the first compound tag (named "data"):
 * @code{c}
 * string tmp = str_create_const("data");
 * nbt_put(&nbt, &name, NBT_COMPOUND);
 * @endcode
 *
 * To add the list, we must select the inner compound tag:
 * @code{c}
 * nbt_move_to_name(&nbt, &name);
 * @endcode
 * Then we add the list:
 * @code{c}
 * name = str_create_const("numbers");
 * nbt_put(&nbt, &name, NBT_LIST);
 * @endcode
 *
 * We add the two numbers :
 * @code {c}
 * nbt_push_simple(&nbt, NBT_INT, (union NBTSimpleValue){.integer = 3});
 * nbt_push_simple(&nbt, NBT_INT, (union NBTSimpleValue){.integer = 27}); // we made a mistake!!!
 * @endcode
 *
 * Before adding the string named "x", we must go up one level :
 * @code{c}
 * nbt_move_to_parent(&nbt);
 * @endcode
 *
 * Then add the string :
 * @code{c}
 * name = str_create_const("x");
 * string value = str_create_const("hello!");
 * nbt_put_str(&nbt, &name, &value);
 * @endcode
 *
 * Oops! We made a mistake when adding the second integer, we have to make it right!
 * To do this, we must select the integer:
 * @code{c}
 * name = str_create_const("numbers");
 * nbt_move_to_name(&nbt, &name);
 * nbt_move_to_index(&nbt, 1);
 * nbt_set_int(&nbt, 7);
 * @endcode
 * And voilà !
 *
 * Complete code:
 * @code{c}
 * Arena arena = arena_create(1 << 15);
 * NBT nbt = nbt_create(&arena, 1024);
 *
 * string name = str_create_const("data");
 * nbt_put(&nbt, &name, NBT_COMPOUND);
 *
 * nbt_move_to_name(&nbt, &tmp);
 *
 * name = str_create_const("numbers");
 * nbt_put(&nbt, &name, NBT_LIST);
 *
 * nbt_push_simple(&nbt, NBT_INT, (union NBTSimpleValue){.integer = 3});
 * nbt_push_simple(&nbt, NBT_INT, (union NBTSimpleValue){.integer = 17});
 *
 * nbt_move_to_parent(&nbt);
 * name = str_create_const("x");
 * string value = str_create_const("hello!");
 * nbt_put_str(&nbt, &name, &value);
 *
 * name = str_create_const("numbers");
 * nbt_move_to_name(&nbt, &name);
 * nbt_move_to_index(&nbt, 1);
 * nbt_set_int(&nbt, 17);
 * @endcode
 *
 * Small tips:
 * - Prefer using @ref nbt_push_simple and @ref nbt_put_simple when adding a number tag.
 *   These functions can set the value of a tag directly after creating it.
 * - You can add any type of tag with @ref nbt_push and @ref nbt_put, but their value
 *   will be initialized to zero / empty.
*/

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
    _NBT_COUNT
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
* @param[in] value A union containing the value to put. Ignored if @ref type is not an integer or floating point number type.
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

/**
* Restores a NBT tree from a file.
*
* This function initializes and populates a new NBT tree by reading a file.
* The file can be compressed using gzip.
*
* @param[in] arena The arena used to allocate memory for the tree.
* @param[in] max_token_count The maximum amount of tags to parse.
* @param[in] path The path to the file to read.
* @param[out] out_nbt A pointer to an uninitialized NBT tree.
* @return @ref TRUE if the parsing completed successfully, @ref FALSE if an error occured.
*/
bool nbt_parse(Arena* arena, i64 max_token_count, const string* path, NBT* out_nbt);

/**
* Moves the current tag pointer to the child tag with the given name.
*
* If the current tag is not a `NBT_COMPOUND` tag, this function does nothing.
*
* @param[in] nbt The NBT tree.
* @param[in] name The name of the child tag to find.
*/
void nbt_move_to_name(NBT* nbt, const string* name);
/**
* Moves the current tag pointer to the child tag at the given index.
*
* If the current tag is not a `NBT_LIST`, `NBT_BYTE_ARRAY`, `NBT_INT_ARRAY` or `NBT_LONG_ARRAY` tag,
* this function does nothing.
*
* @param[in] nbt The NBT tree.
* @param[in] index The index of the child tag to find.
*/
void nbt_move_to_index(NBT* nbt, i32 index);
/**
* Moves the current tag pointer to the parent of the current tag.
*
* If the current tag is the root tag, this function does nothing.
*
* @param[in] nbt The NBT tree.
*/
void nbt_move_to_parent(NBT* nbt);
/**
* Moves the current tag pointer to the next sibling of the current tag.
*
* If the current tag has no sibling or is the last child of a list, array or compound tag,
* this function does nothing.
*
* @param[in] nbt The NBT tree.
*/
void nbt_move_to_next_sibling(NBT* nbt);
/**
* Moves the current tag pointer to the previous sibling of the current tag.
*
* If the current tag has no sibling or is the first child of a list, array or compound tag,
* this function does nothing.
*
* @param[in] nbt The NBT tree.
*/
void nbt_move_to_prev_sibling(NBT* nbt);

/**
* Retrieves the byte value of the current tag.
*
* If the current tag is not a `NBT_BYTE` tag, this function aborts the server.
*
* @param[in] nbt The NBT tree.
* @return The byte value.
*/
i8 nbt_get_byte(NBT* nbt);
/**
* Retrieves the boolean value of the current tag.
*
* This is a convenience function to treat byte tags as booleans.
*
* @param[in] nbt The NBT tree.
* @return The boolean value.
* @see nbt_get_byte(NBT*)
*/
inline bool nbt_get_bool(NBT* nbt) {
    return nbt_get_byte(nbt);
}
/**
* Retrieves the short integer value of the current tag.
*
* If the current tag is not a `NBT_SHORT` tag, this function aborts the server.
*
* @param[in] nbt The NBT tree.
* @return The short integer value.
*/
i16 nbt_get_short(NBT* nbt);
/**
* Retrieves the integer value of the current tag.
*
* If the current tag is not a `NBT_INT` tag, this function aborts the server.
*
* @param[in] nbt The NBT tree.
* @return The integer value.
*/
i32 nbt_get_int(NBT* nbt);
/**
* Retrieves the long integer value of the current tag.
*
* If the current tag is not a `NBT_LONG` tag, this function aborts the server.
*
* @param[in] nbt The NBT tree.
* @return The long integer value.
*/
i64 nbt_get_long(NBT* nbt);
/**
* Retrieves the floating-point value of the current tag.
*
* If the current tag is not a `NBT_FLOAT` tag, this function aborts the server.
*
* @param[in] nbt The NBT tree.
* @return The floating-point value.
*/
f32 nbt_get_float(NBT* nbt);
/**
* Retrieves the double-precision floating-point value of the current tag.
*
* If the current tag is not a `NBT_DOUBLE` tag, this function aborts the server.
*
* @param[in] nbt The NBT tree.
* @return The double-precision floating-point value.
*/
f64 nbt_get_double(NBT* nbt);

/**
* Retrieves the name of the current tag.
*
* If the parent of the current tag is a list or array tag, this functions return `NULL`.
*
* @param[in] nbt The NBT tree.
* @return A pointer to the name of the tag, or NULL if the tag cannot have a name. The returned string can be modified.
*/
string* nbt_get_name(NBT* nbt);
/**
* Retrieves the string value of the current tag.
*
* If the current tag is not a `NBT_STRING` tag, this function aborts the server.
*
* @param[in] nbt The NBT tree.
* @return A pointer to the underlying string value. The string can be modified.
*/
string* nbt_get_string(NBT* nbt);

#endif /* ! NBT_H */
