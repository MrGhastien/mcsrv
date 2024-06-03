#include "utils.h"

#include <stdio.h>
#include <sys/socket.h>

int socket_readbytes(int sockfd, void* restrict buf, size_t byte_count) {
    size_t total_read = 0;
    while (byte_count > 0) {
        size_t immediate_read = recv(sockfd, buf + total_read, byte_count, 0);
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
    while ((byte = buf[i])) {
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
