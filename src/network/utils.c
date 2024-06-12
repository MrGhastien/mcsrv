#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>


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


size_t encode_varint(int n, u8* out) {
    size_t i = 0;
    while (TRUE) {
        out[i] = n & SEGMENT_BITS;
        if (n & ~SEGMENT_BITS)
            out[i] |= CONTINUE_BIT;
        else
            return i + 1;

        n >>= 7;
        i++;
    }
    return 0;
}

void encode_varint_arena(int n, Arena* arena) {
    while (TRUE) {
        u8* byte = arena_allocate(arena, sizeof *byte);
        *byte = n & SEGMENT_BITS;
        if (n & ~SEGMENT_BITS)
            *byte |= CONTINUE_BIT;
        else
            return;

        n >>= 7;
    }
}
