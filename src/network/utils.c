#include "utils.h"
#include "utils/bitwise.h"
#include "network.h"

#include <errno.h>
#include <sys/socket.h>

enum IOCode try_send(int sockfd, void* data, u64 size, u64* out_sent) {
    u64 sent = 0;
    enum IOCode code = IOC_OK;
    while (sent < size) {
        void* begin = offset(data, sent);
        i64 res = send(sockfd, begin, size - sent, 0);

        if (res == -1) {
            code = errno == EAGAIN || errno == EWOULDBLOCK ? IOC_AGAIN : IOC_ERROR;
            break;
        } else if (res == 0) {
            code = IOC_CLOSED;
            break;
        }

        sent += res;
    }
    *out_sent = sent;
    return code;
}

u64 decode_varint(const u8* buf, i32* out) {
    i32 res = 0;
    u64 position = 0;
    u64 i = 0;
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

u64 decode_string(const u8* buf, Arena* arena, string* out_str) {
    i32 length = 0;
    u64 total = decode_varint(buf, &length);
    if (total <= 0)
        return -1;

    *out_str = str_init((const char*)&buf[total], length, length, arena);

    return total + out_str->length;
}

u64 decode_u16(const u8* buf, u16* out) {
    *out = (buf[0] << 8) | buf[1];
    return 2;
}

u64 decode_uuid(const u8* buf, u64* out) {
    u64* cast_buf = (u64*)buf;
    out[0] = cast_buf[0];
    out[1] = cast_buf[1];
    return sizeof(u64) << 1;
}

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
