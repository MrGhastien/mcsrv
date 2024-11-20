#include "utils.h"
#include "network.h"
#include "utils/bitwise.h"

#include <errno.h>

u64 encode_varint(i32 n, u8* buf) {
    u64 i = 0;
    while (i < VARINT_MAX_SIZE) {
        buf[i] = n & SEGMENT_BITS;
        if (n & ~SEGMENT_BITS)
            buf[i] |= CONTINUE_BIT;
        else
            break;
        i++;
        n >>= 7;
    }
    return i + 1;
}

i64 varint_length(i32 n) {
    i64 i = 0;
    while (i < VARINT_MAX_SIZE) {
        if ((n & ~SEGMENT_BITS) != 0)
            break;
        i++;
        n >>= 7;
    }
    if(n != 0)
        return -1;
    return i + 1;
}

static u8 parse_hex_digit(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    else if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    else if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return 16;
}

bool parse_uuid(const string* str, u64* out) {
    if (str->length != 32)
        return FALSE;
    out[0] = 0;
    out[1] = 0;
    u8* buf = (u8*) out;
    for (u64 i = 0; i < str->length; i++) {
        char c = str->base[i];
        u8 digit = parse_hex_digit(c);
        if(digit == 16)
            return FALSE;
        if(i & 1)
            buf[i >> 1] |= digit;
        else
            buf[i >> 1] = digit << 4;
    }
    return TRUE;
}
