#include "containers/bytebuffer.h"
#include "logger.h"
#include "utils/bitwise.h"

#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

ByteBuffer bytebuf_create(u64 size) {
    return (ByteBuffer){
        .buf = malloc(size),
        .read_head = -1,
        .write_head = -1,
        .size = 0,
        .capacity = size,
    };
}

ByteBuffer bytebuf_create_fixed(u64 size, Arena* arena) {
    return (ByteBuffer){
        .buf = arena_allocate(arena, size),
        .read_head = 0,
        .write_head = 0,
        .size = 0,
        .capacity = size,
    };
}

static bool is_fixed(const ByteBuffer* buffer) {
    return buffer->read_head >= 0 && buffer->write_head >= 0;
}

void bytebuf_destroy(ByteBuffer* buffer) {
    if (is_fixed(buffer)) {
        log_fatal("Cannot destroy fixed-size byte buffer.");
        abort();
    }
    free(buffer->buf);
}

static void ensure_capacity(ByteBuffer* buffer, u64 size) {
    if (buffer->capacity >= size)
        return;

    if (is_fixed(buffer)) {
        log_fatalf("Byte buffer is too small: %zu bytes needed, %zu bytes available.",
                   buffer->capacity - buffer->size,
                   size - buffer->size);
        abort();
    }

    u64 new_cap = buffer->capacity;
    while (new_cap < size) {
        new_cap += new_cap >> 1;
    }

    void* new_buf = realloc(buffer->buf, new_cap);
    if (!new_buf) {
        log_fatal("Failed to resize dynamic byte buffer.");
        abort();
    }
    buffer->buf = new_buf;
    buffer->capacity = new_cap;
}

void* bytebuf_reserve(ByteBuffer* buffer, u64 size) {
    if (is_fixed(buffer)) {
        log_error("Cannot reserve a contiguous memory area inside the byte buffer.");
        return NULL;
    }
    ensure_capacity(buffer, buffer->size + size);

    void* ptr = offset(buffer->buf, buffer->size);
    buffer->size += size;
    return ptr;
}

void bytebuf_copy(ByteBuffer *dst, const ByteBuffer *src) {
    bytebuf_write(dst, src->buf, src->size);
}

void bytebuf_write(ByteBuffer* buffer, void* data, size_t size) {
    ensure_capacity(buffer, buffer->size + size);

    u32 end = buffer->write_head;
    u32 cap = buffer->capacity;

    if (is_fixed(buffer)) {
        i64 to_write = end + size > cap ? cap - end : size;
        memcpy(offset(buffer->buf, end), data, to_write);
        if (end + size > cap)
            memcpy(buffer->buf, offset(data, to_write), size - to_write);
        buffer->write_head = (end + size) % buffer->capacity;
    } else
        memcpy(offset(buffer->buf, cap), data, size);

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

void bytebuf_read(ByteBuffer* buffer, u64 size, void* out_data) {
    if (size > buffer->size)
        size = buffer->size;

    if (out_data)
        memcpy(out_data, offset(buffer->buf, buffer->read_head), size);
    buffer->size -= size;
    buffer->read_head = (buffer->read_head + size) % buffer->capacity;
}
