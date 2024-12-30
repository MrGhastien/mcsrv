/**
 * @file
 *
 * Functions related to the string builder utility.
 */

#ifndef STR_BUILDER_H
#define STR_BUILDER_H

#include "containers/vector.h"
#include "utils/string.h"

/**
 * The structure representing a string builder.
 */
typedef struct StringBuilder {
    Vector chars; /**< A vector containing the characters of the builder. */
    Arena* arena; /**< A pointer to the arena used to allocate the character vector. */
} StringBuilder;

/**
 * Creates a new string builder.
 *
 * @param[in] arena The arena to use to allocate the underlying character array.
 * @return The newly created string builder.
 */
StringBuilder strbuild_create(Arena* arena);

/**
 * Appends a single character to a string builder.
 *
 * @param[in] builder The string builder containing the string to append to.
 * @param[in] c The character to append.
 */
void strbuild_appendc(StringBuilder* builder, i32 c);
/**
 * Appends a C string to a string builder.
 *
 * @param[in] builder The string builder containing the string to append to.
 * @param[in] cstr The C string to append.
 */
void strbuild_appends(StringBuilder* builder, const char* cstr);
/**
 * Appends a string to a string builder.
 *
 * @param[in] builder The string builder containing the string to append to.
 * @param[in] str The string to append.
 */
void strbuild_append(StringBuilder* builder, const string* str);

/**
 * Appends a formatted string to a string builder.
 *
 * @param[in] builder The string builder containing the string to append to.
 * @param[in] format The format of the string to append.
 * @param ... Arguments for the format.
 */
void strbuild_appendf(StringBuilder* builder, const char* format, ...);

/**
 * Inserts a single character to a string builder.
 *
 * @param[in] builder The string builder containing the string to append to.
 * @param[in] index The position to insert the given character at.
 * @param[in] c The character to append.
 */
void strbuild_insertc(StringBuilder* builder, u64 index, i32 c);
/**
 * Inserts a C string to a string builder.
 *
 * @param[in] builder The string builder containing the string to append to.
 * @param[in] index The position at which to insert the given C string.
 * @param[in] cstr The C string to append.
 */
void strbuild_inserts(StringBuilder* builder, u64 index, const char* cstr);
/**
 * Inserts a string to a string builder.
 *
 * @param[in] builder The string builder containing the string to append to.
 * @param[in] index The position at which to insert the given string.
 * @param[in] str The string to append.
 */
void strbuild_insert(StringBuilder* builder, u64 index, const string* str);

/**
 * Inserts a formatted string to a string builder.
 *
 * @param[in] builder The string builder containing the string to append to.
 * @param[in] index The position at which to insert the formatted string.
 * @param[in] format The format of the string to insert.
 * @param ... Arguments for the format.
 */
void strbuild_insertf(StringBuilder* builder, u64 index, const char* format, ...);

/**
 * Retrieves a single character at the given in a string builder.
 *
 * If the index is out of bounds, this function returns `-1`.
 *
 * @param[in] builder The builder to query.
 * @param[in] index The position of the character to get.
 * @return The character at index @p index, or `-1` otherwise.
 */
char strbuild_get(const StringBuilder* builder, u64 index);
/**
 * Retrieves a range from a string builder.
 *
 * @param[in] builder The builder to query.
 * @param[out] out_text A pointer to the buffer which will be filled with the characters in the
 * requested range. Should be large enough to hold at least `end - begin` characters.
 * @param[in] begin The position of the first character to retrieve (included)
 * @param[in] end The position of the last character to retrieve (excluded).
 * @return The number of characters retrieved, or `-1` if indices were invalid.
 */
i32 strbuild_get_range(const StringBuilder* builder, char* out_text, u64 begin, u64 end);

/**
 * Creates a new string from the contents of a string builder.
 *
 * @param[in] builder The builder containing the characters of the string to create.
 * @param[in] arena The arena used to allocate the new string.
 * @return A new string containing characters of the builder.
 */
string strbuild_to_string(const StringBuilder* builder, Arena* arena);

#endif /* ! STR_BUILDER_H */
