/**
 * @file
 *
 * String-related functions.
 *
 * The string structure used by these functions represents a length-based string.
 * The implementation also appends a null character at the end of the character array
 * for ease of use with the C standard library functions.
 *
 * The length of strings cannot change, but their contents can. See str_set(), str_copy().
 *
 * Strings can be of two types :
 * - *Plain strings*, which simply contain characters,
 * - *String views*, which are strings which use another string's buffer as its own.
 *   String views do not require memory allocations.
 *
 * Internally, string and string views are the same. String views are created simply by copying the
 * pointer to the character array (i.e. performing a shallow copy).
 */
#ifndef STRING_H
#define STRING_H

#include "definitions.h"
#include "memory/arena.h"
#include "utils/hash.h"

/**
* The string structure.
*/
typedef struct str {
    char* base; /**< The pointer to the character array. Always null-terminated. */
    u64 length; /**< The length of the character array, excluding the null-terminator. */
} string;

/**
* The string comparator.
* @see Comparator
*/
extern const Comparator CMP_STRING;

/**
 * Creates a string from the provided C string,
 * using `arena` to allocate the underlying buffer.
 *
 * The contents are copied.
 *
 * @param[in] cstr The C string containing characters to copy.
 * @param[in] arena The arena used to allocate the new string.
 * @return The newly allocated string containing the characters of @p cstr.
 */
string str_create(const char* cstr, Arena* arena);

/**
 * Creates a string view from the given C string.
 * The contents are NOT copied, instead the given C string is used as a character array directly.
 *
 * @param[in] cstr The source C string. Must be null-terminated.
 * @return A string view containing the characters of @p cstr.
 */
string str_create_view(const char* cstr);

/**
 * Creates a copy of a string.
 *
 * @param[in] str The original string from which to make a copy.
 * @param[in] arena The arena used to allocate the new string's buffer.
 * @return A copy of the passed-in string.
 */
string str_create_copy(const string* str, Arena* arena);

/**
 * Creates a string from a character buffer.
 *
 * This functions differs from str_create() as it operates on arbitrary
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
 * The returned string view will contain all characters from index @p begin included to
 * @p end excluded.
 *
 * @param[in] str The string to get the substring from.
 * @param[in] begin The index of the first character to get (included).
 * @param[in] end The index of the last character to get (excluded).
 * @return A substring view of @p str.
 */
string str_substring(const string* str, u64 begin, u64 end);

/**
* Sets the contents of a string.
*
* This functions does not change the string's length nor does it reallocate the underlying buffer.
*
* If there are more characters in @p cstr than there is available in @p str,
* excess characters are discarded. If there are fewer characters, the string @p str is padded with null
* terminators.
*
* @param[inout] str The string to set.
* @param[in] cstr The contents to set the string to.
*/
void str_set(string* str, const char* cstr);
/**
* Copies the contents of a string into another one.
*
* This function is similar to @ref str_set, except it works with strings.
*
* If there are more characters in @p src than there is available in @p dst,
* excess characters are discarded. If there are fewer characters, the string @p dst is padded with null
* terminators.
*
* @param[inout] dst The string to copy characters to.
* @param[in] src The string to copy characters from.
*/
void str_copy(string* dst, const string* src);

/**
* Creates a new string by concatenating two other strings.
*
* This is a convenience function for a single concatenation of strings.
* Use a @ref str_builder.h "string builder" if multiple concatenations are intended.
*
* @param[in] lhs The left string.
* @param[in] rhs The right string.
* @param[in] arena The arena to use to allocate the resulting string.
* @return A new string containing the concatenation of the two given strings.
*/
string str_concat(string* lhs, const string* rhs, Arena* arena);
/**
* Retrieves a (null-terminated) printable C string  from the specified string.
*
* @param[in] str The string of which to get the contents in a printable C string.
* @return The pointer to the start of the printable C string.
*/
const char* str_printable_buffer(const string* str);

/**
* Computes the hash value of a string.
*
* This function should not be used directly. Instead, use the @ref CMP_STRING @ref Comparator "comparator".
*
* @note This function does not compute a cryptographic hash. It should be used to get a simple hash,
* e.g. to be used with hash tables.
*
* @param[in] str A pointer to a string structure.
* @return The hash as a unsigned 64-bit integer.
* @see Comparator
*/
u64 str_hash(const void* str);
/**
* Compares two strings.
*
* The strings are compared alphabetically.
*
* @param[in] lhs The left string operand.
* @param[in] rhs The right string operand.
* @return `0` if the strings are equal, a negative number if @p lhs is less than @p rhs, and a positive number otherwise.
*/
i32 str_compare(const string* lhs, const string* rhs);

#endif /* ! STRING_H */
