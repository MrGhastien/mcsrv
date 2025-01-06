#include "containers/bytebuffer.h"
#include "logger.h"
#include "network/utils.h"
#include "utils/bitwise.h"
#include "utils/math.h"
#include "utils/string.h"
#include "memory/mem_tags.h"

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

static i64 bytebuf_read_const(const ByteBuffer* buffer, u64 size, void* out_data) {
    if (!out_data)
        return 0;
    if (!is_fixed(buffer)) {
        i64 to_read = min_u64(size, buffer->size);
        memcpy(out_data, buffer->buf, to_read);
        return to_read;
    }

    u64 to_read = min_u64(size, buffer->capacity - buffer->read_head);
    memcpy(out_data, offset(buffer->buf, buffer->read_head), to_read);
    if (to_read < size)
        memcpy(offset(out_data, to_read), buffer->buf, size - to_read);

    return to_read;
}

static bool peek_byte(const ByteBuffer* buffer, u64 index, u8* out_byte) {
    if (buffer->size == 0)
        return 0;
    if (index >= buffer->size)
        return FALSE;

    if (out_byte)
        *out_byte =
            *(u8*) offsetu(buffer->buf, is_fixed(buffer) ? buffer->read_head + index : index);

    return TRUE;
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

static u64 get_read_region_size(const ByteBuffer* buffer, i64 start_offset) {
    if (buffer->size == 0)
        return 0;
    if (buffer->read_head == buffer->write_head)
        return buffer->capacity - start_offset;

    /*
            #2         #3         #1
            v          v          v
        ---------------------------------------------------
        |rrrrrr|wwwwwwwwwwwwwww|rrrrrrrrrrrrrrrrrrrrrrrrrr|
        ---------------------------------------------------
               ^               ^
           write_head      read_head
     */

    if (buffer->read_head > buffer->write_head) {
        if (start_offset >= buffer->read_head)
            return buffer->capacity - start_offset; // #1
        if (start_offset < buffer->write_head)
            return buffer->write_head - start_offset; // #2
        return 0;                                     // #3
    }

    /*
                     #2                #3            #1
                     v                 v             v
        ---------------------------------------------------
        |wwwwwwwwwwwwwwwwwwwwwwwwww|rrrrrrrrrrrr|wwwwwwwww|
        ---------------------------------------------------
                                   ^            ^
                               read_head    write_head
     */

    if (start_offset >= buffer->write_head)
        return 0; // #1
    if (start_offset < buffer->read_head)
        return 0;                             // #2
    return buffer->write_head - start_offset; // #3
}

static u64 get_write_region_size(const ByteBuffer* buffer, i64 start_offset) {
    if (buffer->size == buffer->capacity)
        return 0;
    if (buffer->read_head == buffer->write_head)
        return buffer->capacity - start_offset;

    /*
            #2         #3         #1
            v          v          v
        ---------------------------------------------------
        |rrrrrr|wwwwwwwwwwwwwww|rrrrrrrrrrrrrrrrrrrrrrrrrr|
        ---------------------------------------------------
               ^               ^
           write_head      read_head
     */

    if (buffer->read_head > buffer->write_head) {
        if (start_offset >= buffer->read_head)
            return 0; // #1
        if (start_offset < buffer->write_head)
            return 0;                            // #2
        return buffer->read_head - start_offset; // #3
    }

    /*
                     #2                #3            #1
                     v                 v             v
        ---------------------------------------------------
        |wwwwwwwwwwwwwwwwwwwwwwwwww|rrrrrrrrrrrr|wwwwwwwww|
        ---------------------------------------------------
                                   ^            ^
                               read_head    write_head
     */

    if (start_offset >= buffer->write_head)
        return buffer->capacity - start_offset; // #1
    if (start_offset < buffer->read_head)
        return buffer->read_head - start_offset; // #2
    return 0;                                    // #3
}

static inline bool has_more_regions(const ByteBuffer* buffer, u64 total, bool writable) {
    return writable ? total < buffer->capacity - buffer->size : total < buffer->size;
}

static u64
get_regions(const ByteBuffer* buffer, BufferRegion* out_regions, u64* out_count, bool writable, i64 start_offset) {
    u64 max_size =  writable ? bytebuf_available(buffer) : bytebuf_size(buffer);
    u64 index = 0;
    u64 total = 0;
    if(start_offset < 0)
        start_offset = 0;
    if((u64)start_offset > max_size)
        start_offset = max_size;
    i64 region_start = start_offset + (writable ? buffer->write_head : buffer->read_head);

    while (index < *out_count && has_more_regions(buffer, total, writable)) {
        u64 size = writable ? get_write_region_size(buffer, region_start)
                            : get_read_region_size(buffer, region_start);

        out_regions[index].start = offset(buffer->buf, region_start);
        out_regions[index].size = size;

        region_start = (region_start + size) % buffer->capacity;
        total += size;
        index++;
    }
    *out_count = index;
    return total;
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
        .buf = arena_allocate(arena, size, ALLOC_TAG_BYTEBUFFER),
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

u64 bytebuf_size(const ByteBuffer* buffer) {
    return buffer->size;
}

u64 bytebuf_available(const ByteBuffer* buffer) {
    return buffer->capacity - buffer->size;
}

u64 bytebuf_cap(const ByteBuffer* buffer) {
    return buffer->capacity;
}

i64 bytebuf_current_pos(const ByteBuffer* buffer) {
    i64 res = buffer->write_head - buffer->read_head;
    if(res < 0)
        res += buffer->capacity;
    return res;
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
    u64 region_count = 2;
    BufferRegion regions[2];
    bytebuf_get_read_regions(src, regions, &region_count, 0);

    for(u64 i = 0; i < region_count; i++) {
        bytebuf_write(dst, regions[i].start, regions[i].size);
    }
}

void bytebuf_write(ByteBuffer* buffer, const void* data, size_t size) {
    ensure_capacity(buffer, buffer->size + size);

    if (is_fixed(buffer))
        write_at_position(buffer, data, buffer->write_head, size);
    else
        memcpy(offset(buffer->buf, buffer->size), data, size);

    bytebuf_register_write(buffer, size);
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
    return bytebuf_register_read(buffer, size);
}

i64 bytebuf_read_varint(ByteBuffer* buffer, i32* out) {
    i32 res = 0;
    u64 position = 0;
    i64 i = 0;
    u8 byte;
    while (TRUE) {
        if (!peek_byte(buffer, i, &byte)) {
            return 0;
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
    return bytebuf_register_read(buffer, i);
}

i64 bytebuf_read_mcstring(ByteBuffer* buffer, Arena* arena, string* out_str) {
    i32 length = 0;
    i64 len_byte_count = bytebuf_read_varint(buffer, &length);
    if (len_byte_count <= 0)
        return len_byte_count;

    *out_str = str_create_from_buffer(offset(buffer->buf, buffer->read_head), length, arena);

    return len_byte_count + bytebuf_register_read(buffer, length);
}

i64 bytebuf_peek(const ByteBuffer* buffer, u64 size, void* out_data) {
    if (size > buffer->size)
        size = buffer->size;

    bytebuf_read_const(buffer, size, out_data);
    return size;
}
u64 bytebuf_get_read_regions(const ByteBuffer* buffer, BufferRegion* out_regions, u64* out_count, i64 start_offset) {
    return get_regions(buffer, out_regions, out_count, FALSE, start_offset);
}
u64 bytebuf_get_write_regions(const ByteBuffer* buffer, BufferRegion* out_regions, u64* out_count, i64 start_offset) {
    return get_regions(buffer, out_regions, out_count, TRUE, start_offset);
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
    if (size == 0)
        return;

    // Reading from a dynamic buffer immediately deletes the data.
    if (!is_fixed(buffer)) {
        log_error("Cannot unread data from a dynamic byte buffer.");
        return;
    }

    u64 available = buffer->capacity - buffer->size;
    if (size >= available) {
        buffer->size = buffer->capacity;
        buffer->read_head = buffer->write_head;
        return;
    }

    buffer->size += size;
    buffer->read_head -= size;
    if (buffer->read_head < 0)
        buffer->read_head += buffer->capacity;
}

u64 bytebuf_register_read(ByteBuffer* buffer, u64 size) {

    if (size > buffer->size)
        size = buffer->size;

    if (is_fixed(buffer)) {
        buffer->read_head += size;
        buffer->read_head %= buffer->capacity;
    }
    buffer->size -= size;
    return size;
}

u64 bytebuf_register_write(ByteBuffer* buffer, u64 size) {

    ensure_capacity(buffer, buffer->size + size);
    if (is_fixed(buffer)) {
        buffer->write_head += size;
        buffer->write_head %= buffer->capacity;
    }
    buffer->size += size;
    return size;
}
