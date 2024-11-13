#include "containers/bytebuffer.h"
#include "logger.h"
#include "network/utils.h"
#include "utils/bitwise.h"
#include "utils/math.h"
#include "utils/string.h"

#include <stdlib.h>
#include <string.h>

// === Utility functions ===

static bool is_fixed(const ByteBuffer* buffer) {
    return buffer->read_head >= 0 && buffer->write_head >= 0;
}

static void ensure_capacity(ByteBuffer* buffer, u64 size) {
    if (buffer->capacity >= size)
        return;

    if (is_fixed(buffer)) {
        log_fatalf("Byte buffer is too small: %zu bytes needed, %zu bytes available.",
                   size - buffer->size,
                   buffer->capacity - buffer->size);
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

static void bytebuf_read_const(const ByteBuffer* buffer, u64 size, void* out_data) {
    if (!out_data)
        return;
    if (!is_fixed(buffer)) {
        memcpy(out_data, buffer->buf, min_u64(size, buffer->size));
        return;
    }

    u64 to_read = min_u64(size, buffer->capacity - buffer->read_head);
    memcpy(out_data, offset(buffer->buf, buffer->read_head), to_read);
    if (to_read < size)
        memcpy(offset(out_data, to_read), buffer->buf, size - to_read);
}

static u64 register_read(ByteBuffer* buffer, u64 size) {

    if (size > buffer->size)
        size = buffer->size;

    if (is_fixed(buffer)) {
        buffer->read_head += size;
        buffer->read_head %= buffer->capacity;
    }
    buffer->size -= size;
    return size;
}

static u64 register_write(ByteBuffer* buffer, u64 size) {

    ensure_capacity(buffer, buffer->size + size);
    if (is_fixed(buffer)) {
        buffer->write_head += size;
        buffer->write_head %= buffer->capacity;
    }
    buffer->size += size;
    return size;
}

static void write_at_position(ByteBuffer* buffer, const void* data, i64 position, u64 size) {
    if (!is_fixed(buffer)) {
        log_error("`write_at_position()` shall no be called on dynamic byte buffers.");
        return;
    }
    if (position < -(i64) buffer->capacity || (u64) position > buffer->capacity) {
        log_fatalf("Invalid write position %li.", position);
        abort();
    }

    if (position < 0)
        position += buffer->capacity;

    while (size > 0) {
        u64 writable = min_u64(buffer->capacity - position, size);
        memcpy(offset(buffer->buf, position), data, writable);
        size -= writable;
        position = (position + writable) % buffer->capacity;
    }
}

// === Exported functions ===

// -- Byte Buffer initialization / destruction --

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

void bytebuf_destroy(ByteBuffer* buffer) {
    if (is_fixed(buffer)) {
        log_fatal("Cannot destroy fixed-size byte buffer.");
        abort();
    }
    free(buffer->buf);
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

void bytebuf_copy(ByteBuffer* dst, const ByteBuffer* src) {
    u64 size = min_u64(dst->capacity, src->size);
    bytebuf_read_const(src, size, dst->buf);
    dst->size = size;
    if (is_fixed(dst)) {
        dst->read_head = 0;
        dst->write_head = size;
    }
}

void bytebuf_write_buffer(ByteBuffer* dst, const ByteBuffer* src) {
    ByteBuffer cpy = *src;
    void* tmp;
    u64 size;
    do {
        size = bytebuf_contiguous_read(&cpy, &tmp);
        bytebuf_write(dst, tmp, size);
    } while (size > 0);
}

void bytebuf_write(ByteBuffer* buffer, const void* data, size_t size) {
    ensure_capacity(buffer, buffer->size + size);

    if (is_fixed(buffer))
        write_at_position(buffer, data, buffer->write_head, size);
    else
        memcpy(offset(buffer->buf, buffer->size), data, size);

    register_write(buffer, size);
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

void bytebuf_prepend(ByteBuffer* buffer, const void* data, u64 size) {
    ensure_capacity(buffer, buffer->size + size);

    if (is_fixed(buffer)) {

        buffer->read_head -= size;
        if (buffer->read_head < 0)
            buffer->read_head += buffer->capacity;

        write_at_position(buffer, data, buffer->read_head, size);

    } else {
        memmove(offset(buffer->buf, size), buffer->buf, buffer->size);
        memcpy(buffer->buf, data, size);
    }

    buffer->size += size;
}

void bytebuf_prepend_varint(ByteBuffer* buffer, i32 num) {
    u8 buf[4];
    u64 size = encode_varint(num, buf);

    bytebuf_prepend(buffer, buf, size);
}

i64 bytebuf_read(ByteBuffer* buffer, u64 size, void* out_data) {
    if (size > buffer->size)
        size = buffer->size;

    bytebuf_read_const(buffer, size, out_data);

    if (!is_fixed(buffer))
        memmove(buffer->buf, offset(buffer->buf, size), buffer->size - size);
    return register_read(buffer, size);
}

i64 bytebuf_read_varint(ByteBuffer* buffer, i32* out) {
    i32 res = 0;
    u64 position = 0;
    i64 i = 0;
    u8 byte;
    while (TRUE) {
        if (bytebuf_read(buffer, sizeof byte, &byte) != 1) {
            log_error("Failed to decode varint: Invalid data.");
            return -1;
        }
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

i64 bytebuf_read_mcstring(ByteBuffer* buffer, Arena* arena, string* out_str) {
    i32 length = 0;
    i64 len_byte_count = bytebuf_read_varint(buffer, &length);
    if (len_byte_count == -1)
        return -1;

    *out_str = str_create_from_buffer(offset(buffer->buf, buffer->read_head), length, arena);

    return len_byte_count + register_read(buffer, length);
}

i64 bytebuf_peek(const ByteBuffer* buffer, u64 size, void* out_data) {
    if (size > buffer->size)
        size = buffer->size;

    bytebuf_read_const(buffer, size, out_data);
    return size;
}

u64 bytebuf_contiguous_read(ByteBuffer* buffer, void** out_region) {
    if (!is_fixed(buffer)) {
        *out_region = buffer->buf;
        u64 size = buffer->capacity - buffer->size;
        bytebuf_read(buffer, size, NULL);
        return size;
    }

    *out_region = offset(buffer->buf, buffer->read_head);
    u64 size;

    if (buffer->read_head < buffer->write_head)
        size = buffer->write_head - buffer->read_head;
    else if (buffer->read_head == buffer->write_head)
        size = buffer->size;
    else
        size = buffer->capacity - buffer->read_head;

    return register_read(buffer, size);
}

u64 bytebuf_contiguous_write(ByteBuffer* buffer, void** out_region) {
    if (!is_fixed(buffer)) {
        *out_region = buffer->buf;
        u64 size = buffer->capacity - buffer->size;
        buffer->size += size;
        return size;
    }

    *out_region = offset(buffer->buf, buffer->write_head);
    u64 size;

    if (buffer->read_head < buffer->write_head)
        size = buffer->capacity - buffer->write_head;
    else if (buffer->read_head == buffer->write_head)
        size = buffer->capacity - buffer->size;
    else
        size = buffer->read_head - buffer->write_head;

    return register_write(buffer, size);
}

void bytebuf_unwrite(ByteBuffer* buffer, u64 size) {
    if (size == 0)
        return;

    if (size >= buffer->size) {
        buffer->size = 0;
        if (is_fixed(buffer))
            buffer->write_head = buffer->read_head;
        return;
    }

    buffer->size -= size;
    if (is_fixed(buffer)) {
        buffer->write_head -= size;
        if (buffer->write_head < 0)
            buffer->write_head += buffer->capacity;
    }
}

void bytebuf_unread(ByteBuffer* buffer, u64 size) {
    if(size == 0)
        return;

    // Reading from a dynamic buffer immediately deletes the data.
    if(!is_fixed(buffer)) {
        log_error("Cannot unread data from a dynamic byte buffer.");
        return;
    }

    u64 available = buffer->capacity - buffer->size;
    if(size >= available) {
        buffer->size = buffer->capacity;
        buffer->read_head = buffer->write_head;
        return;
    }

    buffer->size += size;
    buffer->read_head -= size;
    if(buffer->read_head < 0)
        buffer->read_head += buffer->capacity;

}
