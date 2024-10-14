#ifndef UTILS_H
#define UTILS_H

#include "utils/string.h"
#include "definitions.h"

/**
 * Tries sending a buffer of data.
 *
 * @param int sockfd The file descriptor of the socket to send data through.
 * @param[in] data The buffer containing the data to send.
 * @param size The number of bytes in the input buffer.
 * @param[out] out_sent A pointer to a @ref u64, that will contain the number
 *             of bytes sent.
 * @return 
 *         - @ref IOC_OK if all input data has been sent,
 *         - @ref IOC_AGAIN if not all bytes could be sent without waiting,
 *         - @ref IOC_ERROR if a socket error occurred,
 *         - @ref IOC_CLOSED if the connection was closed while sending data.
 */
enum IOCode try_send(int sockfd, void* data, u64 size, u64* out_sent);

/**
 * Encodes an integer into a 32-bit Minecraft VarInt.
 *
 * @param n The number to encode.
 * @param[out] A pointer to the memory that will contain the encoded VarInt.
 * @return The number of bytes written in the output memory. Always greater than 0.
 */
u64 encode_varint(int n, u8* buf);

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
