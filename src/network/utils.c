#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

int socket_readbytes(int sockfd, void* restrict buf, size_t byte_count) {
    size_t total_read = 0;
    while (byte_count > 0) {
        ssize_t immediate_read = recv(sockfd, buf + total_read, byte_count, 0);
        if (immediate_read <= 0)
            return immediate_read;
        byte_count -= immediate_read;
        total_read += immediate_read;
    }
    return total_read;
}

size_t decode_varint(const u8* buf, int* out) {
    int res = 0;
    size_t position = 0;
    size_t i = 0;
    u8 byte;
    while (TRUE) {
        byte = buf[i];
        res |= (byte & SEGMENT_BITS) << position;
        i++;
        position += 7;

        if ((byte & CONTINUE_BIT) == 0)
            break;

        if (position >= 32)
            return -1;
    }

    *out = res;
    return i;
}

size_t decode_string(const u8* buf, Arena* arena, string* out_str) {
    int length = 0;
    size_t total = decode_varint(buf, &length);
    if (total <= 0)
        return -1;

    *out_str = str_init((const char*)&buf[total], length, length, arena);

    return total + out_str->length;
}

size_t decode_u16(const u8* buf, u16* out) {
    *out = (buf[0] << 8) | buf[1];
    return 2;
}
