#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

#include "../utils/string.h"
#include "../definitions.h"

#define CONTINUE_BIT 0x80
#define SEGMENT_BITS 0x7F

#define VARINT_MAX_SIZE 4

int socket_readbytes(int sockfd, void* restrict buf, size_t byte_count);

enum IOCode try_send(int sockfd, void* data, u64 size, u64* out_sent);

size_t decode_varint(const u8* buf, int* out);
size_t decode_string(const u8* buf, Arena* arena, string* out_str);
size_t decode_u16(const u8* buf, u16* out);

u64 encode_varint(int n, u8* buf);

void printd_string(const u8* buf);

#endif /* ! UTILS_H */
