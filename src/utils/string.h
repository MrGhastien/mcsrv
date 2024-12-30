/**
 * @file
 *
 * String-related functions.
 *
 * The string structure used by these functions represents a length-based string.
 * The implementation also appends a null character at the end of the character array
 * for ease of use with the C standard library functions.
 *
 * Strings can be of two types :
 * - *Plain strings*, which simply contain characters,
 * - *String views*, which are strings which use another string's buffer as its own.
 *   String views do not require memory allocations.
 *
 * Internally, string and string views are the same. String views are created.
 */
#ifndef STRING_H
#define STRING_H

#include "definitions.h"
#include "memory/arena.h"
#include "utils/hash.h"
#include <stddef.h>

typedef struct str {
    char* base;
    u64 length;
} string;

extern const Comparator CMP_STRING;

/**
 * Creates a string from the provided C string,
 * using `arena` to allocate the underlying buffer.
 *
 * The contents are copied.
 *
 * @param[in] cstr The C string contaning characters to copy.
 * @param[in] arena The arena used to allocate the new string.
 * @return The newly allocated string containing the characters of @p cstr.
 */
string str_create(const char* cstr, Arena* arena);

/**
 * Creates an immutable string from the given C string.
 * The contents are NOT copied, instead the given C string is used directly.
 *
 * @param[in] cstr The source C string.
 * @return The string containing the characters of @p cstr.
 */
string str_create_const(const char* cstr);

/**
 * Creates a copy of a string.
 *
 * @param[in] str The original string from which to make a copy.
 * @param[in] arena The arena used to allocate the new string's buffer.
 * @return A copy of the passing-in string.
 */
string str_create_copy(const string* str, Arena* arena);

/**
 * Creates a string from a character buffer.
 *
 * This functions differs from str_create() because it operates on arbitrary
 * character buffers. This means it ignores null terminators, and instead uses
 * the buffer's length to know when to stop.
 *
 * @param[in] buf The character buffer to get characters from.
 * @param[in] length The number of characters that are in @p buf.
 * @param[in] arena The arena used to allocate the new string's buffer.
 * @return A new string containing characters of @p buf.
 */
string str_create_from_buffer(const char* buf, u64 length, Arena* arena);

/**
 * Creates a new string of the specified size, but leaves its contents unset.
 *
 * This is a convenience function to use when reading a string from a stream
 * (FILE*, gzFile, socket...).
 * You can also set the contents using @ref str_set.
 *
 * @param[in] length The number of characters to reserve.
 * @param[in] arena The arena to use to allocate the string.
 * @return The newly allocated string.
 */
string str_alloc(u64 length, Arena* arena);
/**
 * Creates a copy of a subset of a string.
 *
 * The returned string will contain all characters from index @p begin included to
 * @p end excluded. The characters are copied to a new string.
 *
 * @param[in] str The string to get the substring from.
 * @param[in] begin The index of the first character to get (included).
 * @param[in] end The index of the last character to get (excluded).
 * @param[in] arena The arena to use to allocate the returned string.
 * @return A substring of @p str.
 */
string str_copy_substring(const string* str, u64 begin, u64 end, Arena* arena);
/**
 * Creates a string view of a subset of another string.
 *
 * The returned string will contain all characters from index @p begin included to
 * @p end excluded. The characters are *NOT* copied !
 *
 * @param[in] str The string to get the substring from.
 * @param[in] begin The index of the first character to get (included).
 * @param[in] end The index of the last character to get (excluded).
 * @return A substring of @p str.
 */
string str_substring(const string* str, u64 begin, u64 end);

void str_set(string* str, const char* cstr);
void str_copy(string* dst, const string* src);

string str_concat(string* lhs, const string* rhs, Arena* arena);
const char* str_printable_buffer(const string* str);

u64 str_hash(const void* str);
i32 str_compare(const string* lhs, const string* rhs);

#endif /* ! STRING_H */
