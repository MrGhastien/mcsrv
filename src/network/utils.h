#ifndef UTILS_H
#define UTILS_H

#include "utils/string.h"
#include "definitions.h"

i32 socket_readbytes(int sockfd, void* restrict buf, u64 byte_count);

enum IOCode try_send(int sockfd, void* data, u64 size, u64* out_sent);

u64 decode_varint(const u8* buf, i32* out);
u64 decode_string(const u8* buf, Arena* arena, string* out_str);
u64 decode_u16(const u8* buf, u16* out);
u64 decode_uuid(const u8* buf, u64* out);

u64 encode_varint(int n, u8* buf);

void printd_string(const u8* buf);

#endif /* ! UTILS_H */
