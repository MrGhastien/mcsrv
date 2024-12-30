/**
 * @file
 *
 * Functions related to the string builder utility.
 */

#ifndef STR_BUILDER_H
#define STR_BUILDER_H

#include "containers/vector.h"
#include "utils/string.h"

typedef struct StringBuilder {
    Vector chars;
    Arena* arena;
} StringBuilder;

/**
 * Creates a new string builder.
 *
 * @param[in] arena The arena to use to allocate the underlying character array.
 * @return The newly created string builder.
 */
StringBuilder strbuild_create(Arena* arena);

/**
 * Appends a single character to the string in a builder.
 *
 * @param[in] builder The string builder containing the string to append to.
 * @param[in] c The character to append.
 */
void strbuild_appendc(StringBuilder* builder, i32 c);
/**
 * Appends a C string to the string in a builder.
 *
 * @param[in] builder The string builder containing the string to append to.
 * @param[in] c The C string to append.
 */
void strbuild_appends(StringBuilder* builder, const char* cstr);
/**
 * Appends a string to the string in a builder.
 *
 * @param[in] builder The string builder containing the string to append to.
 * @param[in] c The string to append.
 */
void strbuild_append(StringBuilder* builder, const string* str);

/**
 * Creates a new string from the contents of a string builder.
 *
 * The returned string's buffer is allocated using the arena passed in to strbuild_create()
 * when the builder is created.
 *
 * @param[in] The builder containing the characters of the string to create.
 * @return A new string containing characters of the builder.
 */
string strbuild_to_string(StringBuilder* builder);

#endif /* ! STR_BUILDER_H */
