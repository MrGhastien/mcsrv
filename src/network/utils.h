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

u64 decode_varint(const u8* buf, i32* out);
u64 decode_string(const u8* buf, Arena* arena, string* out_str);
u64 decode_u16(const u8* buf, u16* out);
u64 decode_uuid(const u8* buf, u64* out);

u64 encode_varint(int n, u8* buf);

void printd_string(const u8* buf);

bool parse_uuid(const string* str, u64* out);

#endif /* ! UTILS_H */
