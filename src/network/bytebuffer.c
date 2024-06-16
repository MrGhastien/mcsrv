#include "bytebuffer.h"
#include "utils.h"
#include "utils/bitwise.h"

#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

ByteBuffer bytebuf_create(u64 size, Arena* arena) {
    return (ByteBuffer){
        .buf      = arena_allocate(arena, size),
        .size     = 0,
        .capacity = size,
    };
}

void* bytebuf_reserve(ByteBuffer* buffer, u64 size) {
    if (buffer->size + size > buffer->capacity)
        abort();

    void* ptr = offset(buffer->buf, buffer->size);
    buffer->size += size;
    return ptr;
}

void bytebuf_write(ByteBuffer* buffer, void* data, size_t size) {
    if (size + buffer->size > buffer->capacity) {
        abort();
        return;
    }

    memcpy(offset(buffer->buf, buffer->size), data, size);
    buffer->size += size;
}

void bytebuf_write_i64(ByteBuffer* buffer, i64 num) {
    num = hton64(num);
    bytebuf_write(buffer, &num, sizeof num);
}

void bytebuf_write_varint(ByteBuffer* buffer, int n) {
    u8 buf[4];
    u64 i = 0;
    while (i < 4) {
        buf[i] = n & SEGMENT_BITS;
        if (n & ~SEGMENT_BITS)
            buf[i] |= CONTINUE_BIT;
        else
            break;
        i++;
        n >>= 7;
    }
    bytebuf_write(buffer, buf, i + 1);
}
