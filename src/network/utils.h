#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

#include "definitions.h"

#define CONTINUE_BIT 0x80
#define SEGMENT_BITS 0x7F

int socket_readbytes(int sockfd, void* restrict buf, size_t byte_count);

size_t decode_varint(const u8* buf, int* out);
size_t decode_string(const u8* buf, char** outp);
size_t decode_u16(const u8* buf, u16* out);

void printd_string(const u8* buf);

#endif /* ! UTILS_H */
