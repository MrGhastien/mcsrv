#ifndef UTILS_H
#define UTILS_H

#include "utils/string.h"
#include "definitions.h"

/**
 * Encodes an integer into a 32-bit Minecraft VarInt.
 *
 * @param n The number to encode.
 * @param[out] A pointer to the memory that will contain the encoded VarInt.
 * @return The number of bytes written in the output memory. Always greater than 0.
 */
u64 encode_varint(int n, u8* buf);

i64 varint_length(i32 n);

/**
 * Extracts an UUID from a given string.
 *
 * An UUID is *always* 16 bytes long.
 * 
 * @param[in] str The string to parse.
 * @param[out] out The output buffer that will contain the parsed UUID.
 * Should be at least 16 bytes long.
 * @return @ref TRUE if a UUID was successfully parsed from the string, @ref FALSE otherwise.
 */
bool parse_uuid(const string* str, u64* out);

#endif /* ! UTILS_H */
